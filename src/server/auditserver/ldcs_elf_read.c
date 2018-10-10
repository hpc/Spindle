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

/**
 * The following code reads a stripped version of an ELF file, rather
 * than the full executable with all debug info and symbols.
 **/
#include <elf.h>
#include <errno.h>
#include <unistd.h>

#include "ldcs_elf_read.h"
#include "ldcs_api.h"

static int readUpTo(FILE *f, unsigned char *buffer, size_t *cur_pos, size_t new_size)
{
   size_t result;
   if (*cur_pos >= new_size)
      return 0;

   do {
      result = fread(buffer + *cur_pos, 1, new_size - *cur_pos, f);
   } while (result == -1 && errno == EINTR);
   if (result == -1)
      return -1;
   *cur_pos += result;
   return 0;
}

#if !defined PT_GNU_RELRO
#define PT_GNU_RELRO 0x6474e552
#endif

#define ERR -1
#define NOT_ELF -2
static int readLoadableFileSections(FILE *f, unsigned char *buffer, size_t *size, int strip)
{
   int result;
   size_t filesize = *size;
   size_t cur_pos = 0;
   size_t ph_start, ph_end;
   unsigned long num_phdrs, i, pagesize, pagediff;
   unsigned long highest_file_addr = 0, cur_file_addr;
   Elf64_Ehdr *ehdr;
   Elf64_Phdr *phdr;

   //Read the first page, which will contain the ELF header
   // (and likely the program headers)
   result = readUpTo(f, buffer, &cur_pos, 0x1000);
   if (result == -1) {
      return ERR;
   }

   //Check that we're a proper elf file.
   if (EI_NIDENT > cur_pos) {
      return NOT_ELF;
   }
   ehdr = (Elf64_Ehdr *) buffer;
   if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
       ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
       ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
       ehdr->e_ident[EI_MAG3] != ELFMAG3) {
      readUpTo(f, buffer, &cur_pos, filesize);
      return NOT_ELF;
   }
   if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
      readUpTo(f, buffer, &cur_pos, filesize);
      return NOT_ELF;
   }
   if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
      readUpTo(f, buffer, &cur_pos, filesize);
      return NOT_ELF;
   }


   //Collect info on program headers and read them into memory
   // (if they weren't read in the last read).
   num_phdrs = ehdr->e_phnum;
   ph_start = ehdr->e_phoff;
   ph_end = ph_start + ehdr->e_phentsize * num_phdrs;
   
   result = readUpTo(f, buffer, &cur_pos, ph_end);
   if (result == -1) {
      return ERR;
   }
   if (cur_pos < ph_end) {
      return ERR;
   }

   //Spindle isn't compatible with PT_GNU_RELRO sections. Delete them
   // by changing the type to an unused type.
   phdr = (Elf64_Phdr *) (buffer + ph_start);
   for (i = 0; i < num_phdrs; i++, phdr++) {
      if (phdr->p_type == PT_GNU_RELRO) {
         phdr->p_type = 0x7a5843cc;
      }
   }

   if (!strip) {
      readUpTo(f, buffer, &cur_pos, filesize);
      return 0;
   }

   //Find the end of the last program header
   phdr = (Elf64_Phdr *) (buffer + ph_start);
   for (i = 0; i < num_phdrs; i++, phdr++) {
      if (phdr->p_type != PT_LOAD)
         continue;
      cur_file_addr = phdr->p_offset + phdr->p_filesz;
      if (cur_file_addr > highest_file_addr)
         highest_file_addr = cur_file_addr;
   }

   //Round up to a page.
   pagesize = getpagesize();
   pagediff = highest_file_addr % pagesize;
   if (pagediff) 
      highest_file_addr += (pagesize - pagediff);
   if (highest_file_addr > filesize)
      highest_file_addr = filesize;

   //Read the main contents of the file
   result = readUpTo(f, buffer, &cur_pos, highest_file_addr);
   if (result == -1) {
      return ERR;
   }
   if (cur_pos < highest_file_addr) {
      return ERR;
   }
   *size = highest_file_addr;
   return 0;
}

int read_file_and_strip(FILE *f, void *data, size_t *size, int strip) {
   int result = readLoadableFileSections(f, (unsigned char *) data, size, strip);
   if (result == ERR) {
      debug_printf3("Error reading from file\n");
      return -1;
   }
   return 0;
}

