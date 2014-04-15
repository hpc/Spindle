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

#include <string>
#include <cassert>
#include <list>
#include <cstring>
#include <sstream>

#include <stdio.h>
#include <limits.h>
#include <link.h>
#include <elf.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "spindle_debug.h"
#include "parse_launcher.h"
#include "config.h"

#if !defined(LIBEXECDIR)
#error Expected LIBEXECDIR to be defined
#endif
static const char openmpi_intercept_lib[] = LIBEXECDIR "/libompiintercept.so";
using namespace std;

typedef unsigned long Address;
class ParseOpenMPIBinary
{
public:
   static ParseOpenMPIBinary *newParser(string path);

   virtual bool parse() = 0;
   virtual Address getMPIRBreakpointAddr() = 0;
   virtual Address getMPIRProctabAddr() = 0;
   virtual Address getMPIRProctabSizeAddr() = 0;
   virtual bool is64() = 0;

   virtual ~ParseOpenMPIBinary();
};

template<typename Elf_Addr, typename Elf_Ehdr, typename Elf_Shdr, typename Elf_Sym>
class ParseSizedBinary : public ParseOpenMPIBinary {
protected:
   Elf_Addr mpir_breakpoint_addr;
   Elf_Addr mpir_proctable_addr;
   Elf_Addr mpir_proctable_size_addr;

   void *mapped_binary;
   size_t binary_size;
   string binary_path;

   bool getSymAddr(string symname, Elf_Shdr *symt, Elf_Shdr *strt, Elf_Addr &addr)
   {
      Elf_Sym *syms = (Elf_Sym *) (symt->sh_offset + (unsigned char *) mapped_binary);
      unsigned int num_syms = symt->sh_size / sizeof(Elf_Sym);
      const char *str_region = (const char *) mapped_binary + strt->sh_offset;

      const char *target = symname.c_str();
      for (unsigned int i = 0; i < num_syms; i++) {
         const char *symname = str_region + syms[i].st_name;
         if (strcmp(target, symname) == 0) {
            addr = syms[i].st_value;
            return true;
         }
      }

      return false;
   }

   bool getSymAddr(string symname, list<pair<Elf_Shdr*, Elf_Shdr*> > &symtabs, Elf_Addr &addr)
   {
      typename std::list<std::pair<Elf_Shdr*, Elf_Shdr*> >::const_iterator i;
      for (i = symtabs.begin(); i != symtabs.end(); i++) {
         bool result = getSymAddr(symname, i->first, i->second, addr);
         if (result) 
            return true;
      }
      err_printf("Failed to find symbol %s in binary %s\n", symname.c_str(), binary_path.c_str());
      return false;
   }

public:
   ParseSizedBinary(string launcher_path, void *mapped_region, size_t size) :
      mpir_breakpoint_addr(0),
      mpir_proctable_addr(0),
      mpir_proctable_size_addr(0),
      mapped_binary(mapped_region),
      binary_size(size),
      binary_path(launcher_path)
   {
   }

   virtual bool parse()
   {
      Elf_Ehdr *hdr = (Elf_Ehdr *) mapped_binary;

      Elf_Shdr *shdrs = (Elf_Shdr *) ((unsigned char *) mapped_binary + hdr->e_shoff);
      unsigned int num_shdrs = (unsigned int) hdr->e_shnum;
      
      list<pair<Elf_Shdr*, Elf_Shdr*> > symtabs;
      for (unsigned int i = 0; i < num_shdrs; i++) {
         Elf_Shdr *hdr = shdrs+i;
         if (hdr->sh_type != SHT_SYMTAB && hdr->sh_type != SHT_DYNSYM)
            continue;
         
         unsigned int strhdr_idx = hdr->sh_link;
         assert(strhdr_idx != 0 && strhdr_idx < num_shdrs);
         Elf_Shdr *strhdr = shdrs + strhdr_idx;

         if (hdr->sh_type == SHT_DYNSYM)
            symtabs.push_front(make_pair(hdr, strhdr));
         else if (hdr->sh_type == SHT_SYMTAB)
            symtabs.push_back(make_pair(hdr, strhdr));
      }

      if (symtabs.empty()) {
         err_printf("No symbol tables found in launcher %s\n", binary_path.c_str());
         fprintf(stderr, "Failed to parse OpenMPI launcher %s for symbols: No symbol tables\n", binary_path.c_str());
         return false;
      }

      bool result = getSymAddr("MPIR_Breakpoint", symtabs, mpir_breakpoint_addr);
      result = result && getSymAddr("MPIR_proctable", symtabs, mpir_proctable_addr);
      result = result && getSymAddr("MPIR_proctable_size", symtabs, mpir_proctable_size_addr);
      if (!result) {
         fprintf(stderr, "Failed to parse OpenMPI launcher %s for symbols: MPIR symbol not found\n",
                 binary_path.c_str());
         return false;
      }
      assert(mpir_breakpoint_addr != 0);
      assert(mpir_proctable_addr != 0);
      assert(mpir_proctable_size_addr != 0);

      debug_printf2("mpir_breakpoint = 0x%lx, mpir_proctable = 0x%lx, mpir_proctable_size = 0x%lx\n",
                    getMPIRBreakpointAddr(), getMPIRProctabAddr(),
                    getMPIRProctabSizeAddr());

      return true;
   }

   virtual Address getMPIRBreakpointAddr() {
      return (Address) mpir_breakpoint_addr;
   }

   virtual Address getMPIRProctabAddr() {
      return (Address) mpir_proctable_addr;
   }

   virtual Address getMPIRProctabSizeAddr() {
      return (Address) mpir_proctable_size_addr;
   }
   
   virtual ~ParseSizedBinary() {
      munmap(mapped_binary, binary_size);
   }
   
   virtual bool is64() {
      return (sizeof(Elf_Addr) == 8);
   }
};

typedef ParseSizedBinary<Elf64_Addr, Elf64_Ehdr, Elf64_Shdr, Elf64_Sym> ParseSizedBinary64;
typedef ParseSizedBinary<Elf32_Addr, Elf32_Ehdr, Elf32_Shdr, Elf32_Sym> ParseSizedBinary32;

ParseOpenMPIBinary *ParseOpenMPIBinary::newParser(string path)
{
   struct stat stat_buf;
   size_t filesize = 0;
   int fd = -1;
   void *mmap_result = NULL;

   try {
      int result = stat(path.c_str(), &stat_buf);
      if (result == -1)
         throw make_pair("stat", strerror(errno));

      filesize = stat_buf.st_size;

      fd = open(path.c_str(), O_RDONLY);
      if (fd == -1) 
         throw make_pair("open", strerror(errno));

      mmap_result = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
      if (mmap_result == MAP_FAILED)
         throw make_pair("mmap", strerror(errno));
   }
   catch (pair<const char *, const char *> err) {
      err_printf("Could not %s openmpi binary %s: %s\n", err.first, path.c_str(), err.second);
      fprintf(stderr, "Failed to parse OpenMPI launcher %s for symbols: %s\n", path.c_str(), err.second);
      
      if (fd != -1)
         close(fd);
      if (mmap_result != NULL && mmap_result != MAP_FAILED)
         munmap(mmap_result, filesize);
      return NULL;
   }
   close(fd);
   
   unsigned char *elf_ident = (unsigned char *) mmap_result;
   if (elf_ident[0] != ELFMAG0 && elf_ident[1] != ELFMAG1 &&
       elf_ident[2] != ELFMAG2 && elf_ident[3] != ELFMAG3) {
      err_printf("%s is not and elf file\n", path.c_str());
      fprintf(stderr, "Failed to parse OpenMPI launcher %s for symbols: Not an elf binary\n", path.c_str());
      return NULL;
   }

   ParseOpenMPIBinary *new_parser = NULL;
   if (elf_ident[EI_CLASS] == ELFCLASS32) {
      debug_printf2("Parsing openmpi launcher symbols from %s as 32-bit elf file\n", path.c_str());
      new_parser = new ParseSizedBinary32(path, mmap_result, filesize);
   }
   else if (elf_ident[EI_CLASS] == ELFCLASS64) {
      debug_printf2("Parsing openmpi launcher symbols from %s as 64-bit elf file\n", path.c_str());
      new_parser = new ParseSizedBinary64(path, mmap_result, filesize);
   }
   else {
      err_printf("Unexpected elf_ident[EI_CLASS] = %d in %s\n", (int) elf_ident[EI_CLASS], path.c_str());
      fprintf(stderr, "Failed to parse OpenMPI launcher %s for symbols: Unknown elf identity\n", path.c_str());
      return NULL;
   }

   return new_parser;
}

ParseOpenMPIBinary::~ParseOpenMPIBinary()
{
}

static string getPreloadStr()
{
   const char *ldpreload = getenv("LD_PRELOAD");
   if (!ldpreload)
      return string(openmpi_intercept_lib);
   return string(openmpi_intercept_lib) + string(" ") + string(ldpreload);
}

bool setOpenMPIInterceptEnv(string launcher_rel)
{
   ExeTest exetest;
   string launcher = exetest.getExecutablePath(launcher_rel);
   if (launcher.empty()) {
      err_printf("Could not find executable path for %s\n", launcher_rel.c_str());
      fprintf(stderr, "Failed to parse OpenMPI launcher %s for symbols: Could not locate launcher executable",
              launcher_rel.c_str());
      return false;
   }
   ParseOpenMPIBinary *parser = ParseOpenMPIBinary::newParser(launcher);
   if (!parser)
      return false;

   bool result = parser->parse();
   if (!result)
      return false;

   char *real_launcher = realpath(launcher.c_str(), NULL);
   if (real_launcher == NULL) {
      err_printf("Error determining realpath of %s: %s\n", launcher.c_str(), strerror(errno));
      return false;
   }
   char *last_slash = strrchr(real_launcher, '/');
   last_slash = last_slash ? last_slash+1 : real_launcher;
   
   stringstream ss;
   ss << last_slash << " " << std::hex << 
      parser->getMPIRBreakpointAddr() << " " << 
      parser->getMPIRProctabAddr() << " " << 
      parser->getMPIRProctabSizeAddr();
   
   setenv("SPINDLE_OMPI_INTERCEPT", ss.str().c_str(), 1);
   
   setenv("LD_PRELOAD", getPreloadStr().c_str(), 1);

   free(real_launcher);
   delete parser;

   return true;
}
