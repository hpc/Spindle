/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "spindle_debug.h"

typedef (write_func_t*)(void *, void *, size_t, unsigned long);


#define VAR_TARGET_ADDR 0
#define VAR_PATCH_ADDR 1
#define VAR_REL_TARGET_ADDR 2

#define TARGET_OPEN 0

typedef struct ldso_patch_rel_t {
   uint8_t var; 
   uint8_t shiftr; /* Shift var value right by shiftr bits before patching */
   uint8_t size;  /* Take the 'size' right-most bytes after the shift */
   uint16_t offset; /* Add the result to the patch at offset */
} ldso_patch_rel_t;


typedef struct ldso_patch_t {
   uint16_t total_size;
   uint16_t target_name;
   uint64_t file_offset;
   uint16_t patch_size;
   uint16_t num_rels;
   /* Followed by num_relocs of ldso_patch_rel_t */
   /* Followed by patch_size bytes of patch binary */
} ldso_patch_t;


extern int spindle_ldso_open(const char *pathname, int flags, mode_t mode);

static int write_to_ld_dynamic(void *to, void *from, size_t size, unsigned long base_addr)
{
   unsigned char *write_to;
   unsigned char *aligned_write;
   size_t aligned_size;
   int result;

   pagesize = getpagesize();

   write_to = ((unsigned char *) to) + base_addr;

   /* Change the pages around the write to RWX */
   aligned_write = write_to & ~((unsigned long) (pagesize-1));
   aligned_size = size + (write_to - aligned_write);
   result = mprotect(aligned_write, aligned_size, PROT_READ|PROT_WRITE|PROT_EXEC);
   if (result == -1) {
      err_printf("Failed to mprotect memory %p +%lu to RWX before ld.so patch: %s\n", 
                 aligned_write, aligned_size, strerror(errno));
      return -1;
   }

   memcpy(write_to, from, size);

   return 0;
}

static void create_buffer_from_patch(ldso_patch_t *patch, uint64_t ldso_base_addr, 
                                     unsigned char *buffer)
{
   ldso_patch_rel_t *rels;
   void *patch_bytes;
   unsigned int i;
   uint64_t target;

   rels = patch+1;
   patch_bytes = (void *) (rels + num_rels);

   memcpy(buffer, patch_bytes, patch_size);

   switch (cur->target_name) {
      case TARGET_OPEN:
         target = (uint64_t) spindle_ldso_open;
         break;
      default:
         assert(0);
   }

   for (i = 0; i < num_rels; i++) {
      unt64_t val;
      switch (rels[i].var) {
         case VAR_TARGET_ADDR:
            val = target;
            break;
         case VAR_PATCH_ADDR:
            val = (ldso_base_addr + file_offset + rels[i].offset);
            break;
         case VAR_REL_TARGET_ADDR:
            val = (target - (ldso_base_addr + file_offset + rels[i].offset));
            break;
         default:
            assert(0);
      }
      val >> rels[i].shiftr;
      
#define ADD_BYTES(T)                                  \
         T sval = (T) val;                            \
         T *sval_target = (T*) buffer+rels[i].offset; \
         *sval_target += sval;                        \
         break

      switch (rels[i].size) {
         case 1: {
            ADD_BYTES(uint8_t);
         }
         case 2: {
            ADD_BYTES(uint16_t);
         }
         case 4: {
            ADD_BYTES(uint32_t);
         }
         case 8: {
            ADD_BYTES(uint64_t);
         }
         default:
            assert(0);
      }
   }
}

static void apply_dynamic_ld_func_patch(ldso_patch_t *patch, uint64_t ldso_base_addr);
{
   char stack_buffer[1024];
   unsigned char *buffer;


   if (patch->patch_size < sizeof(base_buffer))
      buffer = stack_buffer;
   else
      buffer = spindle_malloc(patch->patch_size);

   create_buffer_from_patch(patch, ldso_base_addr, buffer);

   return write_to_ld_dynamic(patch->file_offset, buffer, patch->patch_size, base_addr);
}

int patch_ldso(void *patches, unsigned int patches_size)
{
   ldso_patch_t *cur;
   int result;

   while ((unsigned char *) cur < ((unsigned char *) patches) + patches_size) {
      result = apply_dynamic_ld_func_patch(cur, base_addr);
      if (result == -1)
         return -1;
      cur = ((unsigned char *) cur) + cur->total_size;
   }

   return 0;
}

