/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
<TODO:URL>.

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

#if !defined(parse_launcher_h_)
#define parse_launcher_h_

/* Error returns for createNewCmdLine */
#define NO_LAUNCHER -1
#define NO_EXEC -2

/* Bitmask of values for the test_launchers parameter */
#define TEST_PRESETUP 1<<0
#define TEST_SLURM    1<<1

int createNewCmdLine(int argc, char *argv[],
                     int *new_argc, char **new_argv[],
                     const char *bootstrapper_name,
                     const char *ldcs_location,
                     const char *ldcs_number,
                     unsigned int test_launchers);

#endif
