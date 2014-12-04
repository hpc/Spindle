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

#define ARGV_MAX 1024
#define VARARG_TO_ARGV                                                  \
   char *initial_argv[ARGV_MAX];                                        \
   char **argv = initial_argv, **newp;                                  \
   char **new_argv = NULL;                                              \
   va_list arglist;                                                     \
   int cur = 0;                                                         \
   int size = ARGV_MAX;                                                 \
                                                                        \
   va_start(arglist, arg0);                                             \
                                                                        \
   argv[cur] = (char *) arg0;                                           \
   while (argv[cur++] != NULL) {                                        \
      if (cur >= size) {                                                \
         size *= 2;                                                     \
         if (argv == initial_argv) {                                    \
            argv = (char **) spindle_malloc(sizeof(char *) * size);     \
            if (!argv) {                                                \
               errno = ENOMEM;                                          \
               return -1;                                               \
            }                                                           \
            memcpy(argv, initial_argv, ARGV_MAX*sizeof(char*));         \
         }                                                              \
         else {                                                         \
            newp = (char **) spindle_realloc(argv, sizeof(char *) * size); \
            if (!newp) {                                                \
               spindle_free(argv);                                      \
               errno = ENOMEM;                                          \
               return -1;                                               \
            }                                                           \
         }                                                              \
      }                                                                 \
                                                                        \
      argv[cur] = va_arg(arglist, char *);                              \
   }                                                                    \
    
#define VARARG_TO_ARGV_CLEANUP                                       \
   va_end(arglist);                                                  \
   if (argv != initial_argv) {                                       \
      spindle_free(argv);                                            \
   }                                                                 \
   if (new_argv) {                                                   \
      spindle_free(new_argv);                                        \
   }
