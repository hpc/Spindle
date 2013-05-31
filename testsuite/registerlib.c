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

#if !defined(FUNC_NAME)
#error FUNC_NAME not defined
#endif
#if !defined(SO_NAME)
#error SO_NAME not defined
#endif

#define STR(x) STR2(x)
#define STR2(x) #x

extern int FUNC_NAME();

#if defined(__cplusplus)
extern "C" {
#endif

extern void register_calc_function(int (*func)(void), char *);

static void onload() __attribute__((constructor));
static void onload()
{
   register_calc_function(FUNC_NAME, (char *) STR(SO_NAME));
}

#if defined(__cplusplus)
}
#endif

