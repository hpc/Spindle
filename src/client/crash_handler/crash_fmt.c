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

#include "crash_fmt.h"

#include <stddef.h>

/* This file contains async-signal-safe formatting
   routines for use in the crash handler. The printf family
   of functions are not async-signal-safe. */

static size_t crash_fmt_append_str(char *buf, size_t pos, size_t cap,
                                   const char *s)
{
   size_t i = 0;
   while (s[i]) {
      if (pos + 1 >= cap)
         return (size_t) -1;
      buf[pos++] = s[i++];
   }
   return pos;
}

static size_t crash_fmt_append_hex(char *buf, size_t pos, size_t cap,
                                   unsigned long v)
{
   char tmp[16];
   int i = 0;

   if (v == 0) {
      tmp[i++] = '0';
   } else {
      while (v && i < (int) sizeof(tmp)) {
         unsigned d = (unsigned) (v & 0xFu);
         tmp[i++] = (char) (d < 10 ? '0' + d : 'a' + (d - 10));
         v >>= 4;
      }
   }

   if (pos + (size_t) i + 1 > cap)
      return (size_t) -1;
   while (i > 0)
      buf[pos++] = tmp[--i];
   return pos;
}

/* Generates <library>+<offset> crash site strings. */
int crash_fmt_lib_offset(char *buf, size_t buflen, const char *lib,
                         unsigned long offset)
{
   size_t pos = 0;

   if (buflen == 0)
      return -1;

   if (lib[0] != '\0') {
      pos = crash_fmt_append_str(buf, pos, buflen, lib);
      if (pos == (size_t) -1)
         return -1;
      if (pos + 1 >= buflen)
         return -1;
      buf[pos++] = '+';
   }

   pos = crash_fmt_append_str(buf, pos, buflen, "0x");
   if (pos == (size_t) -1)
      return -1;

   pos = crash_fmt_append_hex(buf, pos, buflen, offset);
   if (pos == (size_t) -1)
      return -1;

   if (pos >= buflen)
      return -1;
   buf[pos] = '\0';
   return 0;
}
