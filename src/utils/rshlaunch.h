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

#if !defined(RSHLAUNCH_H_)
#define RSHLAUNCH_H_

#include "spindle_launch.h"

#if defined(__cplusplus)
extern "C" {
#endif
   
void init_rsh_launch_fe(spindle_args_t *args);
int collect_rsh_pid_fe();
void init_rsh_launch_be(int argc, char **argv);
char *get_rsh_launcher();
pid_t get_fe_rsh_pid();
void clear_fe_rsh_pid();

#if defined(__cplusplus)
}
#endif

#endif
