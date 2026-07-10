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

#ifndef CRASH_FUNCTIONS_H
#define CRASH_FUNCTIONS_H

void crash_function_A(int rank);
void crash_function_B(int rank);

#define CRASH_TABLE_SIZE 64

typedef void (*crash_fn_t)(int);
extern crash_fn_t crash_table[CRASH_TABLE_SIZE];

void static_crash(int rank);

#endif
