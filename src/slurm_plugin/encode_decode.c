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
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include "plugin_utils.h"

#define INT32_CHAR_LEN 12
#define INT64_CHAR_LEN 22
char *encodeSpindleConfig(uint32_t port, uint32_t num_ports, uint64_t unique_id, uint32_t security_type,
                          int spindle_argc, char **spindle_argv)
{
   int result;
   char *configstr = NULL;
   int len = 0, used = 0, i;
   char *ret = NULL;
   
   //five ints, one long, 6 seperator tokens, NIL
   len += INT32_CHAR_LEN*5 + INT64_CHAR_LEN + 6 + 1;
   for (i = 0; i < spindle_argc; i++) {
      len += strlen(spindle_argv[i]); //String
      len += INT32_CHAR_LEN;          //String len
      len += 2;                       //Seperator tokens
   }
   configstr = (char *) malloc(len);
   
   result = snprintf(configstr, len, "%u:%u:%lu:%u:%d", port, num_ports,
                     unique_id, security_type, spindle_argc);
   if (result == -1) {
      sdprintf(1, "ERROR: Formating error encoding configstr params\n");
      goto done;
   }
   used += result;

   for (i = 0; i < spindle_argc; i++) {
      result = snprintf(configstr+used, len-used, ":%u:%s", (int) strlen(spindle_argv[i]), spindle_argv[i]);
      if (result == -1) {
         sdprintf(1, "ERROR: Formating error encoding configstr args: %s += %u:%s\n", configstr,
                  (int) strlen(spindle_argv[i]), spindle_argv[i]);
         goto done;
      }
      used += result;
   }
   configstr[used++] = '\0';
   assert(used < len);

   ret = configstr;
   sdprintf(2, "Encoded configuration string as %s\n", configstr);
  done:
   if (!ret && configstr)
      free(configstr);
   return ret;
}

int decodeSpindleConfig(const char *encodedstr,
                        unsigned int *port, unsigned int *num_ports, uint64_t *unique_id, uint32_t *security_type,
                        int *spindle_argc, char ***spindle_argv)
{
   int ret = -1, pos = 0, bytes_read, result, encoded_len, strsize, i;
   char *s;

   sdprintf(2, "Decoding configuration string %s\n", encodedstr);
   
   *spindle_argv = NULL;
   encoded_len = strlen(encodedstr);

   result = sscanf(encodedstr, "%u:%u:%lu:%u:%d%n", port, num_ports, unique_id, security_type, spindle_argc, &bytes_read);
   if (result != 5) {
      sdprintf(1, "ERROR: Could not decode parameters (%d)\n", result);
      goto done;
   }
   pos += bytes_read;
   if (pos > encoded_len) {
      sdprintf(1, "ERROR: formating error (%d, %d)\n", pos, encoded_len);
      goto done;
   }
   if (*spindle_argc < 0 || *spindle_argc >= 1024) {
      sdprintf(1, "ERROR: Suspicious argc: %d\n", *spindle_argc);
      goto done;
   }
   *spindle_argv = (char **) calloc((*spindle_argc)+1, sizeof(char*));
   for (i = 0; i < *spindle_argc; i++) {
      result = sscanf(encodedstr + pos, ":%d:%n", &strsize, &bytes_read);
      if (result != 1) {
         sdprintf(1, "ERROR: Could not decode string size for arg %d\n", i);
         goto done;
      }
      if (strsize < 0 || strsize > 4096) {
         sdprintf(1, "ERROR: Suspicious string size %d for arg %d\n", strsize, i);
         goto done;
      }
      pos += bytes_read;
      if (pos > encoded_len) {
         sdprintf(1, "ERROR: formating error (%d, %d)\n", pos, encoded_len);
         goto done;         
      }
      s = (char *) malloc(strsize+1);
      memcpy(s, encodedstr+pos, strsize);
      pos += strsize;
      s[strsize] = '\0';
      (*spindle_argv)[i] = s;
   }
   (*spindle_argv)[*spindle_argc] = NULL;
   ret = 0;
  done:
   if (ret == -1 && *spindle_argv) {
      for (i = 0; i < *spindle_argc; i++) {
         if ((*spindle_argv)[i] != NULL)
            free((*spindle_argv)[i]);
      }
      free(*spindle_argv);
   }
   return ret;
}
