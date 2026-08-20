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

#if !defined(CRASH_ARCH_H_)
#define CRASH_ARCH_H_

#include "config.h"

#include <ucontext.h>
#if defined(arch_aarch64)
#include <asm/sigcontext.h>
#endif

static inline unsigned long extract_pc(void *uctx)
{
   ucontext_t *uc = (ucontext_t *) uctx;
#if defined(arch_x86_64)
   return (unsigned long) uc->uc_mcontext.gregs[REG_RIP];
#elif defined(arch_aarch64)
   return (unsigned long) uc->uc_mcontext.pc;
#elif defined(arch_ppc64) || defined(arch_ppc64le)
   return (unsigned long) uc->uc_mcontext.gp_regs[32];
#else
#error "extract_pc: unsupported architecture"
#endif
}

/* Unlike x86-64 and ppc64le, for aarch64, whether the fault was a read or a write
 * is not found in a default register as a direct field of uc_mcontext.
 * The relevant bit is found in the ESR (Exception Syndrome Register).
 * The register is accessed by iterating over the extended context in __reserved 
 * to find the tag ESR_MAGIC; then the next entry is the esr_context.
 * See https://blog.linuxplumbersconf.org/2017/ocw/system/presentations/4671/original/plumbers-dm-2017.pdf 
 * for info on the aarch64 extended context, and see
 * https://developer.arm.com/documentation/ddi0595/2020-12/AArch64-Registers/ESR-EL1--Exception-Syndrome-Register--EL1-
 * for info on the Exception Syndrome Register. */
#if defined(arch_aarch64)
static inline const struct _aarch64_ctx *first_aarch64_ctx(const ucontext_t *uc)
{
   return (const struct _aarch64_ctx *) uc->uc_mcontext.__reserved;
}

static inline const struct _aarch64_ctx *next_aarch64_ctx(const struct _aarch64_ctx *hdr)
{
   return (const struct _aarch64_ctx *) ((const char *) hdr + hdr->size);
}

static inline int esr_is_write(unsigned long long esr)
{
   return ((esr >> 27) & 0x1f) == 0x12 && ((esr >> 6) & 1) != 0;
}
#endif /* arch_aarch64 */

static inline int pf_is_write(void *uctx)
{
   ucontext_t *uc = (ucontext_t *) uctx;
#if defined(arch_x86_64)
   return (uc->uc_mcontext.gregs[REG_ERR] & 0x2UL) != 0;
#elif defined(arch_aarch64)
   for (const struct _aarch64_ctx *hdr = first_aarch64_ctx(uc); hdr->magic; hdr = next_aarch64_ctx(hdr)) {
      if (hdr->magic == ESR_MAGIC)
         return esr_is_write(((const struct esr_context *) hdr)->esr);
   }
   return 0;
#elif defined(arch_ppc64) || defined(arch_ppc64le)
   return (uc->uc_mcontext.gp_regs[42] & 0x02000000UL) != 0;
#else
#error "pf_is_write: unsupported architecture"
#endif
}

#endif
