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

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "ldcs_api.h"

#if defined(__cplusplus)
extern "C" {
#endif
   char *parse_location(char *loc);
#if defined(__cplusplus)
}
#endif

#define IS_ENVVAR_CHAR(X) ((X >= 'a' && X <= 'z') || (X >= 'A' && X <= 'Z') || (X == '_'))

/* Expand the user specified location with environment variable values */
char *parse_location(char *loc)
{
   char newloc[MAX_PATH_LEN+1];
   char envvar[MAX_PATH_LEN+1];
   int i = 0, j = 0;
   int is_escaped = 0;
   
   while (loc[i] != '\0') {
      if (j >= MAX_PATH_LEN) {
         fprintf(stderr, "Spindle Error: Path length of location %s is too long\n", loc);
         err_printf("Evaluated path was too large: %s\n", loc);
         return NULL;
      }
      if (is_escaped) {
         switch (loc[i]) {
            case 'a': newloc[j] = '\a'; break;
            case 'b': newloc[j] = '\b'; break;
            case 'f': newloc[j] = '\f'; break;
            case 'n': newloc[j] = '\n'; break;
            case 'r': newloc[j] = '\r'; break;
            case 't': newloc[j] = '\t'; break;
            case 'v': newloc[j] = '\v'; break;
            default: newloc[j] = loc[i];
         }
         is_escaped = 0;
         i++;
         j++;
      }
      else if (loc[i] == '\\') {
         is_escaped = 1;
         i++;
      }
      else if (loc[i] == '$') {
         char *env_start = loc + i + 1;
         char *env_end = env_start;
         size_t envvar_len, env_value_len;
         char *env_value;

         while (IS_ENVVAR_CHAR(*env_end)) env_end++;
         envvar_len = env_end - env_start;
         if (envvar_len > MAX_PATH_LEN) {
            fprintf(stderr, "Spindle Error: Environment variable in location string is too large: %s\n",
                    loc);
            err_printf("Environment variable too long in: %s\n", loc);
            return NULL;
         }
         strncpy(envvar, env_start, envvar_len);
         envvar[envvar_len] = '\0';

         env_value = getenv(envvar);
         if (!env_value) {
            fprintf(stderr, "Spindle Error: No environment variable '%s' defined, from specified location %s\n",
                    envvar, loc);
            err_printf("Could not find envvar %s in %s\n", envvar, loc);
            return NULL;
         }
         env_value_len = strlen(env_value);
         if (env_value_len + j > MAX_PATH_LEN) {
            fprintf(stderr, "Spindle Error: Location path %s is too long when evaluated\n", loc);
            err_printf("Location path %s too long when adding envvar %s and value %s\n", loc, envvar, env_value);
            return NULL;
         }
         strncpy(newloc + j, env_value, env_value_len);
         i += envvar_len + 1;
         j += env_value_len;
      }
      else {
         newloc[j] = loc[i];
         i++;
         j++;
      }
   }

   newloc[j] = '\0';
   return strdup(newloc);
}

