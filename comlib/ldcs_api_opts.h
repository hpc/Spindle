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

#if !defined(LDCS_API_OPTS_H_)
#define LDCS_API_OPTS_H_

#define OPT_COBO       (1 << 1)
#define OPT_DEBUG      (1 << 2)
#define OPT_FOLLOWFORK (1 << 3)
#define OPT_PRELOAD    (1 << 4)
#define OPT_PUSH       (1 << 5)
#define OPT_PULL       (1 << 6)
#define OPT_RELOCAOUT  (1 << 7)
#define OPT_RELOCSO    (1 << 8)
#define OPT_RELOCEXEC  (1 << 9)
#define OPT_RELOCPY    (1 << 10)
#define OPT_STRIP      (1 << 11)
#define OPT_NOCLEAN    (1 << 12)

extern unsigned long opts;

#endif
