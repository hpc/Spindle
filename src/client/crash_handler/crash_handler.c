/*
This file is part of Spindle.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms
and conditions of the GNU Lesser General Public License for more details.  You should
have received a copy of the GNU Lesser General Public License along with this
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>

#include "config.h"
#include "ldcs_api.h"
#include "client.h"
#include "client_api.h"
#include "crash_handler.h"
#include "crash_arch.h"
#include "crash_fmt.h"
#include "crash_io.h"
#include "crash_lib_offset.h"
#include "crash_sigchain.h"

#define CRASH_ALTSTACK_SIZE 65536
/* Payload has to fit in server's receive buffer which is size MAX_PATH_LEN */
#define CRASH_SITE_BUF_SIZE (MAX_PATH_LEN - 3 * sizeof(int32_t))
#define CRASH_REQ_BUF_SIZE \
    (sizeof(ldcs_message_header_t) + 3 * sizeof(int32_t) + CRASH_SITE_BUF_SIZE)
#define CRASH_ABORT_MSG_MAX  (64u * 1024u)

static int  crash_global_rank  = -1;
static int  crash_display_rank = -1;
static int  crash_read_fd      = -1;
static int  crash_write_fd     = -1;
static int  crash_installed    = 0;

static char *crash_altstack_buf = NULL;
static char crash_site_buf[CRASH_SITE_BUF_SIZE];

static volatile sig_atomic_t handler_active = 0;

/* abort_msg_s is not defined in any glibc public header
   so we provide our own here. If glibc changes this in
   a future version, this will have to change here too. */
struct abort_msg_s {
   unsigned int size;
   char         msg[];
};

/* Given the program counter pc, puts <library>+<offset>
 into buffer buf of size buflen */
static void resolve_pc_to_crash_site(unsigned long pc, char *buf, size_t buflen)
{
   if (buflen == 0) return;
   buf[0] = '\0';
   if (crash_lib_offset_get_signal_safe(pc, buf, buflen) == 0)
      return;
   (void) crash_fmt_lib_offset(buf, buflen, "", pc);
}

/* Builds the crash report message. */
static size_t build_crash_report(char *buf, size_t buflen, int rank,
                                 int display_rank, const char *site)
{
   size_t name_len = strlen(site) + 1;
   size_t payload_len = 3 * sizeof(int32_t) + name_len;
   size_t total_len = sizeof(ldcs_message_header_t) + payload_len;
   if (total_len > buflen)
      return 0;

   ldcs_message_header_t hdr;
   hdr.type = LDCS_MSG_CRASH_REPORT;
   hdr.len  = payload_len;
   memcpy(buf, &hdr, sizeof(hdr));

   char *payload = buf + sizeof(hdr);
   int32_t rank32 = (int32_t) rank;
   int32_t drank32 = (int32_t) display_rank;
   int32_t nlen32 = (int32_t) name_len;
   memcpy(payload, &rank32, sizeof(rank32));
   memcpy(payload + sizeof(int32_t), &drank32, sizeof(drank32));
   memcpy(payload + 2 * sizeof(int32_t), &nlen32, sizeof(nlen32));
   memcpy(payload + 3 * sizeof(int32_t), site, name_len);
   return total_len;
}

/* Attempts to read the value of the libc __abort_msg into
   buffer buf of size buflen. */
static size_t read_abort_msg(char *buf, size_t buflen)
{
   struct abort_msg_s *volatile *abort_msg_loc = (struct abort_msg_s *volatile *) get_libc_abort_msg();

   if (buflen == 0) return 0;
   if (abort_msg_loc == NULL) return 0; // We failed to find the symbol

   struct abort_msg_s *p = *abort_msg_loc;
   if (p == NULL) return 0;
   if (p->size <= sizeof(unsigned int)) return 0; // if not bigger than the header, there is no abort_msg
   if (p->size > CRASH_ABORT_MSG_MAX) return 0;

   size_t msg_size = (size_t) p->size - sizeof(unsigned int);
   size_t cap = msg_size < buflen - 1 ? msg_size : buflen - 1;
   size_t n = 0;
   while (n < cap && p->msg[n] != '\0') {
      buf[n] = p->msg[n];
      n++;
   }
   buf[n] = '\0';
   return n;
}

/* Prefix crash site with executable to distinguish crashes at the same site
   but from different executables within different jobs of the same session. */
static size_t crash_write_exe_prefix(char *buf, size_t buflen)
{
   static const char trunc_mark[] = "...";
   const size_t mark_len = sizeof(trunc_mark) - 1;
   const char *exe = crash_lib_offset_exe_path();
   size_t exe_len = strlen(exe);
   size_t max_exe = buflen / 2;
   size_t pos = 0;

   if (max_exe <= mark_len + 1)
      return 0;
   max_exe -= 1;   /* room for '|' */

   if (exe_len > max_exe) {
      memcpy(buf, trunc_mark, mark_len);
      pos = mark_len;
      exe += exe_len - (max_exe - mark_len);
      exe_len = max_exe - mark_len;
   }
   memcpy(buf + pos, exe, exe_len);
   pos += exe_len;
   buf[pos++] = '|';
   return pos;
}

/* Builds the crash site string <executable>|<site>, where <site> is the
   abort_msg for SIGABRT and <library>+<offset> otherwise. */
static void crash_build_site(int sig, unsigned long pc,
                             char *buf, size_t buflen)
{
   if (buflen == 0) return;
   buf[0] = '\0';

   size_t prefix_len = crash_write_exe_prefix(buf, buflen);
   char *site = buf + prefix_len;
   size_t site_buflen = buflen - prefix_len;

   if (sig == SIGABRT) {
      static const char abort_prefix[] = "abort:";
      const size_t abort_len = sizeof(abort_prefix) - 1;
      size_t n = 0;

      if (site_buflen > abort_len) {
         memcpy(site, abort_prefix, abort_len);
         n = read_abort_msg(site + abort_len, site_buflen - abort_len);
      }
      // if we failed to get the abort string, fall back to <library>+<offset>
      if (n == 0)
         resolve_pc_to_crash_site(pc, site, site_buflen);
   } else {
      resolve_pc_to_crash_site(pc, site, site_buflen);
   }
}

/* Send the CRASH_REQUEST to the server and read back the CRASH_RESPONSE.
   We can't use the normal send/recv here because we're in a signal handler,
   so instead do raw read/write to pipe. */
static int crash_query_server(const char *site, int rank, int32_t *winner)
{
   char req_buf[CRASH_REQ_BUF_SIZE];
   size_t req_len = build_crash_report(req_buf, sizeof req_buf, rank,
                                       crash_display_rank, site);
   if (req_len == 0)
      return -1;

   if (crash_raw_write(crash_write_fd, req_buf, req_len) != 0)
      return -1;

   ldcs_message_header_t resp_hdr;
   if (crash_raw_read_exact(crash_read_fd, &resp_hdr, sizeof resp_hdr) != 0)
      return -1;
   if (resp_hdr.type != LDCS_MSG_CRASH_RESPONSE ||
       resp_hdr.len  != sizeof(int32_t))
      return -1;

   int32_t winning_rank = -1;
   if (crash_raw_read_exact(crash_read_fd, &winning_rank,
                            sizeof winning_rank) != 0)
      return -1;

   *winner = winning_rank;
   return 0;
}

/* This is the main crash handler. Chains to application handler, if
   installed, and checks whether the application fixed the fault.
   If not, we are going to crash, and we report the crash and determine
   whether we are the winning rank that will write the coredump for
   this specific crash site. */
static void crash_handler_entry(int sig, siginfo_t *info, void *uctx)
{
   /* Reentrancy flag to detect if we crash again while handling the crash. */
   static __thread sig_atomic_t reentering = 0;

   /* We need to restore errno before returning. */
   int saved_errno = errno;

   /* Get the faulting address. */
   unsigned long pc_before = extract_pc(uctx);

   /* If the application registered its own signal handler, call it. */
   int chained = crash_sigchain_chain_to_app(sig, info, uctx);

   /* We got past the application's signal handler.
      If we refault past this point, we crashed in the crash handler;
      in that case, give up and exit. */
   if (reentering) {
      _exit(128 + sig);
   }
   reentering = 1;

   /* Check whether the application's signal handler resolved the fault.
      If it did, we return and let the instruction re-execute. */
   if (chained && crash_sigchain_fault_resolved(sig, info, uctx, pc_before)) {
      reentering = 0;
      errno = saved_errno;
      return;
   }

   /* If we reach this point, the application did NOT fix the issue, so we
      know this is a real crash. Now we set the handler_active flag.
      This ensures that only the first crash to make it here goes through the
      deduplication process. (Since whether to dump is a process-wide decision,
      we can't make a different decision for different threads.)
      Faulting threads other than the first one pause forever until we're done
      with this signal handler, at which point the process will terminate. */
   if (!__sync_bool_compare_and_swap(&handler_active, 0, 1)) {
      for (;;) pause();
   }

   if (!crash_installed || crash_write_fd < 0 || crash_read_fd < 0) {
      goto reraise;
   }

   /* Now we do the actual deduplication part. We get the program counter,
      resolve it to <library>+<offset> or abort_msg, and pass that crashsite
      to the server, which picks one winner per crashsite. */
   unsigned long pc = extract_pc(uctx);
   crash_build_site(sig, pc, crash_site_buf, sizeof crash_site_buf);

   int32_t winning_rank = -1;
   if (crash_query_server(crash_site_buf, crash_global_rank,
                          &winning_rank) != 0)
      goto reraise;

   if ((int) winning_rank != crash_global_rank) {
      /* If we are NOT the winner, we set our own core limit to zero,
         preventing us from dumping. If we are the winner, do nothing,
         preserving the existing core limit. */
      struct rlimit no_core = { 0, 0 };
      (void) setrlimit(RLIMIT_CORE, &no_core);
   }

reraise:
   /* Finally, we restore the default signal handler and return,
      terminating the process and producing a coredump if the limit allows.. */
   signal(sig, SIG_DFL);
}

/* Resolves a display rank for use in crash logging from launcher/MPI env vars. */
static int resolve_display_rank(int fallback)
{
   static const char *const rank_vars[] = {
      "PMIX_RANK", "OMPI_COMM_WORLD_RANK", "PMI_RANK", "JSM_NAMESPACE_RANK",
      "FLUX_TASK_RANK", "MV2_COMM_WORLD_RANK", "PALS_RANKID", "ALPS_APP_PE",
      "SLURM_PROCID"
   };
   for (size_t i = 0; i < sizeof(rank_vars) / sizeof(rank_vars[0]); i++) {
      const char *val = getenv(rank_vars[i]);
      if (val == NULL || val[0] == '\0')
         continue;
      char *end = NULL;
      long rank = strtol(val, &end, 10);
      if (*end != '\0' || rank < 0 || rank > INT32_MAX)
         continue;
      debug_printf2("display rank %ld from %s\n",
                    rank, rank_vars[i]);
      return (int) rank;
   }
   debug_printf2("could not detect MPI rank from environment\n");
   return fallback;
}

/* Performs setup and installs the signal handler. */
int crash_handler_install(int global_rank, int ldcsid_in)
{
   if (crash_installed)
      return 0;

   crash_sigchain_init();

   crash_global_rank = global_rank;
   crash_display_rank = resolve_display_rank(global_rank);

   if (client_get_raw_fds(ldcsid_in, &crash_read_fd, &crash_write_fd) != 0 ||
       crash_read_fd < 0 || crash_write_fd < 0) {
      err_printf("failed to get raw FDs for crash handler\n");
      crash_read_fd = -1;
      crash_write_fd = -1;
      return 0;
   }

   crash_lib_offset_prime();

   /* Perform a dummy resolution to populate the cache. */
   {
      char tmp[256];
      resolve_pc_to_crash_site((unsigned long) &crash_handler_install,
                           tmp, sizeof tmp);
   }

   /* Set up the altstack if enabled.
      If the reason for a segfault is a stack overflow, the signal handler itself
      will have no stack available. We handle this by registering an alternate stack
      for the signal handler if requested with --crash-altstack.
      However, note that this is per-thread, and currently we do not register an
      alternate stack on any thread other than the main thread.
      TODO: handle alternate stack on other threads */
   if (opts & OPT_CRASH_ALTSTACK) {
      crash_altstack_buf = mmap(NULL, CRASH_ALTSTACK_SIZE, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (crash_altstack_buf == MAP_FAILED) {
         crash_altstack_buf = NULL;
         debug_printf2("crash handler: failed to mmap altstack\n");
      } else {
         stack_t ss;
         memset(&ss, 0, sizeof ss);
         ss.ss_sp = crash_altstack_buf;
         ss.ss_size = CRASH_ALTSTACK_SIZE;
         ss.ss_flags = 0;
         if (sigaltstack(&ss, NULL) != 0) {
            munmap(crash_altstack_buf, CRASH_ALTSTACK_SIZE);
            crash_altstack_buf = NULL;
            debug_printf2("crash handler: failed to register altstack\n");
         } else {
            debug_printf2("crash handler: registered altstack\n");
         }
      }
   } else {
      crash_altstack_buf = NULL;
   }

   /* Install the signal handler. */
   struct sigaction sa;
   memset(&sa, 0, sizeof sa);
   sa.sa_sigaction = crash_handler_entry;
   sa.sa_flags     = SA_SIGINFO | SA_RESTART;
   if (crash_altstack_buf != NULL)
      sa.sa_flags |= SA_ONSTACK;
   sigemptyset(&sa.sa_mask);
   sigaddset(&sa.sa_mask, SIGSEGV);
   sigaddset(&sa.sa_mask, SIGBUS);
   sigaddset(&sa.sa_mask, SIGFPE);
   sigaddset(&sa.sa_mask, SIGILL);
   sigaddset(&sa.sa_mask, SIGABRT);

   const int sigs[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT };
   for (size_t i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) {
      /* If a signal handler was already installed by the time Spindle
         registers its signal handler, we save it to use in
         signal handler chaining. */
      struct sigaction handler_old;
      memset(&handler_old, 0, sizeof handler_old);
      if (sigaction(sigs[i], &sa, &handler_old) != 0) {
         debug_printf("sigaction failed when installing crash handler for signal %d\n", sigs[i]);
         continue;
      }
      crash_sigchain_register_existing_handler(sigs[i], &handler_old);
   }

   crash_installed = 1;
   return 0;
}
