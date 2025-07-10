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

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

#if !defined(USE_PLUGIN_DEBUG)
#include "spindle_debug.h"
#else
#include "plugin_utils.h"
#define SPINDLE_DEBUG_H_
#endif

#include "ldcs_api.h"
#include "ccwarns.h"
#include "spindle_launch.h"

//char *parse_location(char *loc, number_t number);
extern int spindle_mkdir(char *orig_path);

#if defined(CUSTOM_GETENV)
extern char *custom_getenv();
#endif
#define IS_ENVVAR_CHAR(X) ((X >= 'a' && X <= 'z') || (X >= 'A' && X <= 'Z') || (X == '_'))

/* Expand the user specified location with environment variable values */
static char *parse_location_impl(char *loc, number_t number, int print_on_error)
{
   char newloc[MAX_PATH_LEN+1];
   char envvar[MAX_PATH_LEN+1];
   int i = 0, j = 0;
   int is_escaped = 0;
   

   debug_printf("Entering parse_location_impl: loc='%s', number=%ld\n", loc, number);
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
         char env_value_str[32];
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
         if (strcmp(envvar, "NUMBER") == 0) {
            snprintf(env_value_str, sizeof(env_value_str), "%lx", (unsigned long) number);
            env_value = env_value_str;
         }
         else {
#if defined(CUSTOM_GETENV)
            env_value = custom_getenv(envvar);
#else
            env_value = getenv(envvar);
#endif
         }
         if (!env_value) {
            if (print_on_error) {
               fprintf(stderr, "Spindle Error: No environment variable '%s' defined, from specified location %s\n",
                       envvar, loc);
               err_printf("Could not find envvar %s in %s\n", envvar, loc);
               return NULL;
            }
            else {
               debug_printf("Warning, could not find envvar %s in %s\n", envvar, loc);
               return strdup("");
            }
         }
         env_value_len = strlen(env_value);
         if (env_value_len + j > MAX_PATH_LEN) {
            fprintf(stderr, "Spindle Error: Location path %s is too long when evaluated\n", loc);
            err_printf("Location path %s too long when adding envvar %s and value %s\n", loc, envvar, env_value);
            return NULL;
         }
         GCC8_DISABLE_WARNING("-Wstringop-overflow"); //Warns we truncate NULL byte.  That's on purpose.
         GCC8_DISABLE_WARNING("-Wstringop-truncation"); 
         strncpy(newloc + j, env_value, env_value_len);
         GCC8_ENABLE_WARNING;
         GCC8_ENABLE_WARNING; 
         i += envvar_len + 1;
         j += env_value_len;
#if defined(CUSTOM_GETENV) && defined(CUSTOM_GETENV_FREE)
         free(env_value);
#endif
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

char *parse_location(char *loc, number_t number)
{
   return parse_location_impl(loc, number, 1);
}

char *parse_location_noerr(char *loc, number_t number)
{
   return parse_location_impl(loc, number, 0);
}

/**
 * Realize takes the 'realpath' of a non-existant location.
 * If later directories in the path don't exist, it'll cut them
 * off, take the realpath of the ones that do, then append them
 * back to the resulting realpath.
 **/
char *realize(char *path)
{
   int local_errno;
   char *result;
   char *origpath, *cur_slash = NULL, *prev_slash = NULL;
   struct stat *buf = calloc( 1, sizeof( struct stat ) );
   char newpath[MAX_PATH_LEN+1];
   int lastpos;
   newpath[MAX_PATH_LEN] = '\0';

   origpath = strdup(path);
   errno=0;
   while( stat( origpath, buf ) == -1 ){
      local_errno = errno;
      debug_printf("Failed to stat '%s' (%s).\n", origpath, strerror(local_errno));
      prev_slash = cur_slash;
      cur_slash = strrchr(origpath, '/');
      if( prev_slash )
          *prev_slash = '/';
      if( cur_slash )
          *cur_slash = '\0';
      else{
          err_printf("Nothing in the original path can be stat'ed.  (%s)\n", path);
          assert(0);
      }
      errno=0;
   }

   errno = 0;
   result = realpath(origpath, newpath);
   if (!result) {
      local_errno = errno;
      err_printf(
          "Error:  realpath(3) failed to create canonical version of '%s' (%s).  Returning '%s'.\n",
          origpath, strerror(local_errno), path );
      errno = 0;
      int rc = stat( origpath, buf );
      local_errno = errno;
      err_printf(
          "        Statting that path results in rc=%d, errno=%d, error='%s'.\n",
          rc, local_errno, strerror(local_errno));
      free(origpath);
      assert(0);
   }
   free(buf);

   if( cur_slash ){
       if( strlen( newpath ) + strlen( cur_slash+1 ) > MAX_PATH_LEN ){
            err_printf(
                    "Error:  The realized path exceeds MAX_PATH_LEN (%d).\n"
                    "  Original path:     '%s'\n"
                    "  Statable part:     '%s'\n"
                    "  Canonical version: '%s'\n"
                    "  Returning original path.\n",
                    MAX_PATH_LEN, path, origpath, newpath);
            free(origpath);
            return path;
       }
       strncat(newpath, "/",         2);
       strncat(newpath, cur_slash+1, MAX_PATH_LEN - strlen( newpath ));
   }
   free(origpath);

   lastpos = strlen(newpath)-1;
   if (lastpos >= 0 && newpath[lastpos] == '/')
      newpath[lastpos] = '\0';

   debug_printf2("Realized %s to %s\n", path, newpath);
   return strdup(newpath);
}

/**
 * parse_colonsep_prefixes turns a colon-seperated list of paths into an array of paths.
 * Number is the spindle session number and is used to expand $NUMBER. Other environment 
 * variables in the paths are expanded normally.
 **/
char **parse_colonsep_prefixes(char *colonsep_list, number_t number)
{
   char **prefixes;
   char *s = strdup(colonsep_list);
   
   if (s == NULL || s[0] == '\0') {
      prefixes = (char **) malloc(sizeof(char *));
      prefixes[0] = NULL;
      return prefixes;
   }
   size_t numprefixes = 1;
   for (size_t i = 0; s[i] != '\0'; i++) {
      if (s[i] == ':') {
         numprefixes++;
      }
   }   
   size_t num_strs = numprefixes + 1;
   
   prefixes = (char **) malloc(sizeof(char*) * num_strs);
   size_t i = 0, cur = 0;
   while (s[i] != '\0') {
      while (s[i] == ':') {
         s[i] = '\0';
         i++;
      }
      if (s[i] == '\0')
         break;
      prefixes[cur] = s+i;
      cur++;
      while (s[i] != ':' && s[i] != '\0')
         i++;
   }
   prefixes[cur] = NULL;    
   cur++;
   assert(cur <= num_strs);

   for (i = 0; prefixes[i] != NULL; i++) {
      prefixes[i] = parse_location_noerr(prefixes[i], number);
   }
   
   free(s);
   return prefixes;
}

/**
 * is_local_prefix takes an array of local prefixes, local_prefixes, and
 * returns true if path is prefixed by one of the local prefixes.
 **/   
int is_local_prefix(const char *path, char **local_prefixes) {
   if (!path || path[0] != '/')
      return 0;
   if (!local_prefixes)
      return 0;
   
   for (int i = 0; local_prefixes[i] != NULL; i++) {
      size_t len = strlen(local_prefixes[i]);
      if (len == 0)
         continue;
      if (strncmp(path, local_prefixes[i], len) == 0) {
         return 1;
      }
   }
   return 0;
}

/**
 * Iterates through a colon-separated list of candidate paths in origPathList along with the session
 * number and attempts to create each path.
 *
 * If not NULL, symbolicPath will contain a pointer to the symbolic version of the first valid path.
 *  That is to say, environment variables in the path will not be expanded.
 * If not NULL, parsedPath will contain a pointer to a version of that path with environment variables
 *  substituted with their values, e.g., $TMP.
 * If not NULL, realizedPath will contain a pointer to a canonical version of the path, e.g.,
 *  symbolic links replaced with the actual directory names.
 * If no valid paths are found, the values in realizedPath, parsedPath, and symbolicPath will be
 *  unchanged.
 */
void parsePaths( char **realizedPath, char **parsedPath, char **symbolicPath, char *origPathList, number_t number ){

    char * pathList = strdup( origPathList );
    char *saveptr, *candidatePath, *parsedCandidatePath, *realizedCandidatePath;
    int rc;

    debug_printf2("origPathList='%s', number='%lu'.\n", origPathList, number );
    candidatePath = strtok_r( pathList, ":", &saveptr );
    while( NULL != candidatePath ){
        parsedCandidatePath = parse_location( candidatePath, number );
        if( parsedCandidatePath ){
           realizedCandidatePath = realize( parsedCandidatePath );
           rc = spindle_mkdir( parsedCandidatePath );
           if( 0 == rc ){
               if( symbolicPath) *symbolicPath = candidatePath;
               if( parsedPath  ) *parsedPath   = parsedCandidatePath;
               if( realizedPath) *realizedPath = realizedCandidatePath;
               return;

           }else{
               debug_printf2("Unable to create directory %s, moving on to the next candidate.\n", parsedCandidatePath );
           }
        }else{
            debug_printf2("Unable to parse candidate %s, moving on to the next candidate.\n", candidatePath );
        }
        candidatePath = strtok_r( NULL, ":", &saveptr );
    }
}
