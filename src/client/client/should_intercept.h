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

#if !defined(SHOULD_INTERCEPT_H_)
#define SHOULD_INTERCEPT_H_

/**
 * These functions are the control our policy on whether we relocate operations
 * through spindle, or let them happen normally
 **/

#define ORIG_CALL 0
#define REDIRECT 1
#define EXCL_OPEN 2
#define ERR_CALL 3

int open_filter(const char *fname, int flags);
int fopen_filter(const char *fname, const char *flags);
int exec_filter(const char *fname);
int stat_filter(const char *fname);
int fd_filter(int fd);

#endif
