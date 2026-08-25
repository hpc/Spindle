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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "ldcs_api.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_md.h"
#include "ldcs_audit_server_crash_handler.h"
#include "spindle_launch.h"
#include "msgbundle.h"

static char *crash_pack(int32_t rank, const char *site, size_t site_len,
                        size_t *out_len);
static char *crash_pack_report(int32_t dedup_rank, int32_t display_rank,
                               const char *site, size_t site_len,
                               const char *corepath, size_t *out_len);
static void send_crash_response_to_waiter(ldcs_process_data_t *procdata,
                                          crash_waiter_t *w,
                                          int winning_rank,
                                          const char *site, size_t site_len);
static int crash_resolve(ldcs_process_data_t *procdata,
                         crash_site_entry_t *entry, int winning_rank);
static int forward_crash_query_up(ldcs_process_data_t *procdata,
                                  const char *site, size_t site_len,
                                  const char *corepath,
                                  int first_waiter_rank,
                                  int first_waiter_display_rank);
static int crash_parse_report(ldcs_message_t *msg, const char *err_str,
                              int32_t *dedup_rank, int32_t *display_rank,
                              const char **site, size_t *site_len,
                              const char **corepath);
static int crash_parse_response(ldcs_message_t *msg, const char *err_str,
                                int32_t *rank,
                                const char **site, size_t *site_len);
static int crash_report_common(ldcs_process_data_t *procdata,
                               crash_waiter_t *w, int reporter_rank,
                               int reporter_display_rank,
                               const char *site, size_t site_len,
                               const char *corepath,
                               crash_site_entry_t **entry_out);

/* INTERNAL HELPER FUNCTIONS */

crash_site_entry_t *crash_site_find(ldcs_process_data_t *procdata,
                                    const char *site, size_t site_len)
{
   int i;
   for (i = 0; i < procdata->crash_sites_count; ++i) {
      crash_site_entry_t *e = &procdata->crash_sites[i];
      if (e->site_len == site_len &&
          memcmp(e->site, site, site_len) == 0) {
         return e;
      }
   }
   return NULL;
}

crash_site_entry_t *crash_site_insert(ldcs_process_data_t *procdata,
                                      const char *site, size_t site_len)
{
   if (procdata->crash_sites_count >= procdata->crash_sites_cap) {
      int new_cap = procdata->crash_sites_cap ? procdata->crash_sites_cap * 2 : 8;
      procdata->crash_sites = realloc(procdata->crash_sites,
                                      new_cap * sizeof(*procdata->crash_sites));
      procdata->crash_sites_cap = new_cap;
   }
   char *copy = malloc(site_len + 1);
   memcpy(copy, site, site_len);
   copy[site_len] = '\0';
   crash_site_entry_t *e = &procdata->crash_sites[procdata->crash_sites_count++];
   memset(e, 0, sizeof(*e));
   e->site = copy;
   e->site_len = site_len;
   e->exemplar_rank = -1;
   return e;
}

static char *crash_pack(int32_t rank, const char *site, size_t site_len,
                        size_t *out_len)
{
   size_t total = 2 * sizeof(int32_t) + site_len;
   char *buf = malloc(total);
   int32_t name_len_v = (int32_t) site_len;
   memcpy(buf, &rank, sizeof(int32_t));
   memcpy(buf + 1 * sizeof(int32_t), &name_len_v, sizeof(int32_t));
   memcpy(buf + 2 * sizeof(int32_t), site, site_len);
   *out_len = total;
   return buf;
}

static char *crash_pack_report(int32_t dedup_rank, int32_t display_rank,
                               const char *site, size_t site_len,
                               const char *corepath, size_t *out_len)
{
   size_t corepath_len = corepath ? strlen(corepath) + 1 : 0;
   size_t total = 4 * sizeof(int32_t) + site_len + corepath_len;
   char *buf = malloc(total);
   int32_t name_len_v = (int32_t) site_len;
   int32_t corepath_len_v = (int32_t) corepath_len;
   char *p = buf;
   memcpy(p, &dedup_rank, sizeof(int32_t));        p += sizeof(int32_t);
   memcpy(p, &display_rank, sizeof(int32_t));      p += sizeof(int32_t);
   memcpy(p, &name_len_v, sizeof(int32_t));        p += sizeof(int32_t);
   memcpy(p, site, site_len);                      p += site_len;
   memcpy(p, &corepath_len_v, sizeof(int32_t));    p += sizeof(int32_t);
   memcpy(p, corepath, corepath_len);
   *out_len = total;
   return buf;
}

static void send_crash_response_to_waiter(ldcs_process_data_t *procdata,
                                          crash_waiter_t *w,
                                          int winning_rank,
                                          const char *site, size_t site_len)
{
   switch (w->kind) {
      case CRASH_WAITER_LOCAL: {
         // Delivering a crash response to a local client
         int32_t resp = (int32_t) winning_rank;
         ldcs_message_t msg;
         msg.header.type = LDCS_MSG_CRASH_RESPONSE;
         msg.header.len  = sizeof(resp);
         msg.data = (char *) &resp;
         ldcs_client_t *client = procdata->client_table + w->nc;
         if (client->state == LDCS_CLIENT_STATUS_ACTIVE && client->connid >= 0) {
            debug_printf2("delivering CRASH_RESPONSE to local nc=%d rank=%d winning=%d\n",
                         w->nc, w->global_rank, winning_rank);
            ldcs_send_msg(client->connid, &msg);
         }
         break;
      }
      case CRASH_WAITER_CHILD: {
         // Delivering a crash response to a child
         size_t len = 0;
         char *buf = crash_pack((int32_t) winning_rank, site, site_len, &len);
         ldcs_message_t msg;
         msg.header.type = LDCS_MSG_CRASH_RESPONSE;
         msg.header.len  = len;
         msg.data = buf;
         debug_printf2("delivering CRASH_RESPONSE to child peer=%p winning=%d\n",
                      (void *) w->peer, winning_rank);
         spindle_send(procdata, &msg, w->peer);
         free(buf);
         break;
      }
      default:
         err_printf("unknown crash waiter kind %d for site '%.*s'\n",
                    (int) w->kind, (int) site_len, site);
         break;
   }
}

static int crash_resolve(ldcs_process_data_t *procdata,
                         crash_site_entry_t *entry, int winning_rank)
{
   debug_printf2("delivering site '%s' winning=%d\n",
                entry->site, winning_rank);
   send_crash_response_to_waiter(procdata, &entry->waiter,
                                 winning_rank, entry->site, entry->site_len);
   entry->resolved = 1;
   return 0;
}

static int forward_crash_query_up(ldcs_process_data_t *procdata,
                                  const char *site, size_t site_len,
                                  const char *corepath,
                                  int first_waiter_rank,
                                  int first_waiter_display_rank)
{
   size_t total = 0;
   char *buf = crash_pack_report((int32_t) first_waiter_rank,
                                 (int32_t) first_waiter_display_rank,
                                 site, site_len, corepath, &total);
   ldcs_message_t msg;
   msg.header.type = LDCS_MSG_CRASH_REPORT;
   msg.header.len  = total;
   msg.data = buf;
   int rc = spindle_forward_query(procdata, &msg);
   free(buf);
   return rc;
}

static int crash_parse_report(ldcs_message_t *msg, const char *err_str,
                              int32_t *dedup_rank, int32_t *display_rank,
                              const char **site, size_t *site_len,
                              const char **corepath)
{
   if (msg->header.len < 3 * sizeof(int32_t)) {
      err_printf("malformed crash report header in %s\n", err_str);
      return -1;
   }
   int32_t name_len;
   memcpy(dedup_rank, msg->data, sizeof(int32_t));
   memcpy(display_rank, msg->data + 1 * sizeof(int32_t), sizeof(int32_t));
   memcpy(&name_len, msg->data + 2 * sizeof(int32_t), sizeof(int32_t));
   if (name_len <= 0 || (size_t) name_len > msg->header.len - 3 * sizeof(int32_t)) {
      err_printf("bad crash report name_len %d in %s\n", (int) name_len, err_str);
      return -1;
   }
   *site = msg->data + 3 * sizeof(int32_t);
   *site_len = (size_t) name_len;

   if (corepath)
      *corepath = NULL;
   size_t used = 3 * sizeof(int32_t) + (size_t) name_len;
   if (used == msg->header.len)
      return 0;
   int32_t cp_len;
   if (msg->header.len - used < sizeof(int32_t)) {
      err_printf("bad crash report core path header in %s\n", err_str);
      return -1;
   }
   memcpy(&cp_len, msg->data + used, sizeof(int32_t));
   used += sizeof(int32_t);
   if (cp_len < 0 || (size_t) cp_len != msg->header.len - used ||
       (cp_len > 0 && msg->data[msg->header.len - 1] != '\0')) {
      err_printf("bad crash report core path length %d in %s\n", (int) cp_len, err_str);
      return -1;
   }
   if (cp_len > 0 && corepath)
      *corepath = msg->data + used;
   return 0;
}

static int crash_parse_response(ldcs_message_t *msg, const char *err_str,
                                int32_t *rank,
                                const char **site, size_t *site_len)
{
   if (msg->header.len < 2 * sizeof(int32_t)) {
      err_printf("malformed crash response in %s\n", err_str);
      return -1;
   }
   int32_t name_len;
   memcpy(rank, msg->data, sizeof(int32_t));
   memcpy(&name_len, msg->data + 1 * sizeof(int32_t), sizeof(int32_t));
   if (name_len <= 0) {
      err_printf("bad name_len %d in %s\n", (int) name_len, err_str);
      return -1;
   }
   *site = msg->data + 2 * sizeof(int32_t);
   *site_len = (size_t) name_len;
   return 0;
}

static int crash_report_common(ldcs_process_data_t *procdata,
                               crash_waiter_t *w, int reporter_rank,
                               int reporter_display_rank,
                               const char *site, size_t site_len,
                               const char *corepath,
                               crash_site_entry_t **entry_out)
{
   // If we have already seen this crash site before, then we know
   // it can't be the winner and can short-circuit and respond
   // immediately that this rank was not selected.
   crash_site_entry_t *e = crash_site_find(procdata, site, site_len);
   if (e) {
      if (entry_out)
         *entry_out = e;
      debug_printf2("known crash site '%s' (%s); suppressing %s reporter rank=%d display=%d\n",
                   e->site, e->resolved ? "resolved" : "in flight",
                   w->kind == CRASH_WAITER_LOCAL ? "local" : "child",
                   reporter_rank, reporter_display_rank);
      send_crash_response_to_waiter(procdata, w, -1, site, site_len);
      return 0;
   }

   // If we haven't seen this crash site before, remember that we have
   // seen it and forward up the tree for resolution.
   e = crash_site_insert(procdata, site, site_len);
   e->waiter = *w;
   if (entry_out)
      *entry_out = e;

   // If we reached the root without finding a decision already made,
   // then this was the first instance of this crash site;
   // select this rank to produce the exemplar coredump.
   if (ldcs_audit_server_md_is_responsible(procdata, "")) {
      debug_printf2("new crash site '%s' at root; selecting rank %d (display %d)\n",
                   e->site, reporter_rank, reporter_display_rank);
      if (procdata->opts & OPT_CRASH_LOG) {
         e->exemplar_rank = reporter_display_rank;
         // The reporter is the exemplar, so its predicted core file is the site's core file
         if (corepath)
            e->exemplar_corepath = strdup(corepath);
      }
      return crash_resolve(procdata, e, reporter_rank);
   }

   debug_printf2("new crash site '%s' at interior; forwarding upward %d (display %d)\n",
                e->site, reporter_rank, reporter_display_rank);
   return forward_crash_query_up(procdata, site, site_len, corepath,
                                 reporter_rank, reporter_display_rank);
}

/* PUBLIC API */

// Stash the executable path or predicted core path that a local client
// sends ahead of its CRASH_REPORT. Messages from the client to the server
// are limited to 4096 bytes, so these are sent as separate messages
// since each can be a path of up to 4096 bytes by itself.
int handle_client_crash_string(ldcs_process_data_t *procdata,
                               int nc, ldcs_message_t *msg)
{
   if (!(procdata->opts & OPT_CRASH_HANDLER)) {
      return 0;
   }

   ldcs_client_t *client = procdata->client_table + nc;
   char **slot;
   switch (msg->header.type) {
      case LDCS_MSG_CRASH_EXE:
         slot = &client->crash_exe;
         break;
      case LDCS_MSG_CRASH_COREPATH:
         slot = &client->crash_corepath;
         break;
      default:
         err_printf("unexpected message type %s from local client nc=%d in crash string handler\n",
                    _message_type_to_str(msg->header.type), nc);
         return -1;
   }
   if (msg->header.len == 0 || msg->data[msg->header.len - 1] != '\0') {
      err_printf("malformed %s from local client nc=%d (len=%lu)\n",
                 _message_type_to_str(msg->header.type), nc,
                 (unsigned long) msg->header.len);
      return -1;
   }
   free(*slot);
   *slot = malloc(msg->header.len);
   memcpy(*slot, msg->data, msg->header.len);
   debug_printf2("Received %s '%s' from local client nc=%d\n",
                 _message_type_to_str(msg->header.type), *slot, nc);
   return 0;
}

void crash_free_client_stash(ldcs_client_t *client)
{
   free(client->crash_exe);
   client->crash_exe = NULL;
   free(client->crash_corepath);
   client->crash_corepath = NULL;
}

// Act on a crash report received from a local client
int handle_client_crash_report(ldcs_process_data_t *procdata,
                               int nc, ldcs_message_t *msg)
{
   if (!(procdata->opts & OPT_CRASH_HANDLER)) {
      return 0;
   }

   debug_printf2("Received CRASH_REPORT from local client nc=%d (len=%lu)\n",
                 nc, (unsigned long) msg->header.len);

   int32_t rank_raw;
   int32_t display_rank;
   const char *site;
   size_t site_len;
   if (crash_parse_report(msg, "crash report from local client", &rank_raw, &display_rank,
                          &site, &site_len, NULL) != 0)
      return -1;

   // The dedup key is <executable>|<site>
   ldcs_client_t *client = procdata->client_table + nc;
   if (!client->crash_exe) {
      err_printf("CRASH_REPORT from local client nc=%d without a preceding CRASH_EXE; dropping\n", nc);
      return -1;
   }
   size_t exe_len = strlen(client->crash_exe);
   size_t key_len = exe_len + 1 + site_len;   /* site_len counts the site's NUL */
   char *key = malloc(key_len);
   memcpy(key, client->crash_exe, exe_len);
   key[exe_len] = '|';
   memcpy(key + exe_len + 1, site, site_len);
   key[key_len - 1] = '\0';

   crash_waiter_t w;
   w.kind = CRASH_WAITER_LOCAL;
   w.nc = nc;
   w.global_rank = (int) rank_raw;
   w.peer = NULL;
   crash_site_entry_t *e = NULL;
   int result = crash_report_common(procdata, &w, (int) rank_raw, (int) display_rank,
                                    key, key_len, client->crash_corepath, &e);

   if ((procdata->opts & OPT_CRASH_LOG) && e) {
      crash_log_append_rank(e, display_rank);
      debug_printf2("crash log: recorded local display rank %d at site '%s' (%d ranks)\n",
                    (int) display_rank, e->site, e->log_ranks_count);
      crash_log_updated(procdata);
   }
   free(key);
   crash_free_client_stash(client);
   return result;
}

// Act on a crash report passed to us from a child
int handle_crash_report_recv(ldcs_process_data_t *procdata,
                             node_peer_t peer, ldcs_message_t *msg)
{
   // Reports only ever flow up the tree, so this must be from a child
   if (ldcs_audit_server_md_is_parent(peer)) {
      err_printf("unexpectedly got CRASH_REPORT from parent\n");
      return -1;
   }

   int32_t first_waiter_rank;
   int32_t first_waiter_display_rank;
   const char *site, *corepath;
   size_t site_len;
   if (crash_parse_report(msg, "crash report from child", &first_waiter_rank, &first_waiter_display_rank,
                          &site, &site_len, &corepath) != 0)
      return -1;

   crash_waiter_t w;
   w.kind = CRASH_WAITER_CHILD;
   w.nc = -1;
   w.global_rank = -1;
   w.peer = peer;
   return crash_report_common(procdata, &w, (int) first_waiter_rank,
                              (int) first_waiter_display_rank, site, site_len,
                              corepath, NULL);
}

// Act on a crash response passed to us from our parent
int handle_crash_response_recv(ldcs_process_data_t *procdata,
                               node_peer_t peer, ldcs_message_t *msg)
{
   if (!ldcs_audit_server_md_is_parent(peer)) {
       err_printf("unexpectedly got CRASH_RESPONSE from peer other than parent\n");
       return -1;
   }
    
   int32_t selected_rank;
   const char *site;
   size_t site_len;
   if (crash_parse_response(msg, "crash response from parent", &selected_rank, &site, &site_len) != 0)
      return -1;

   crash_site_entry_t *e = crash_site_find(procdata, site, site_len);
   if (!e) {
       err_printf("unexpectedly got CRASH_RESPONSE for unknown site %.*s\n", (int)site_len, site);
       return -1;
   }
   if (e->resolved) {
      err_printf("unexpectedly got CRASH_RESPONSE for already resolved site %.*s\n", (int)site_len, site);
      return -1;
   }
   debug_printf2("received CRASH_RESPONSE site '%.*s' selected=%d\n", (int) site_len, site, (int) selected_rank);
    
   return crash_resolve(procdata, e, (int) selected_rank);
}

// Free the tables that store cached crash-site decisions.
void crash_free_tables(ldcs_process_data_t *procdata)
{
   int i;
   if (procdata->crash_sites) {
      for (i = 0; i < procdata->crash_sites_count; ++i) {
         free(procdata->crash_sites[i].site);
         free(procdata->crash_sites[i].exemplar_corepath);
         free(procdata->crash_sites[i].log_ranks);
      }
      free(procdata->crash_sites);
      procdata->crash_sites = NULL;
      procdata->crash_sites_count = 0;
      procdata->crash_sites_cap = 0;
   }
}
