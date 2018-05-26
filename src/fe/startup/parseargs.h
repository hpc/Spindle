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

#if !defined(PARSEARGS_H_)
#define PARSEARGS_H_

#include "spindle_launch.h"
#include <string>

typedef enum {
   sstatus_unused,
   sstatus_start,
   sstatus_run,
   sstatus_end
} session_status_t;

void parseCommandLine(int argc, char *argv[], spindle_args_t *args);

opt_t parseArgs(int argc, char *argv[]);
char *getPreloadFile();
unsigned int getPort();
unsigned int getNumPorts();
std::string getLocation(int number);
std::string getPythonPrefixes();
std::string getHostbin();
int getStartupType();
int getLauncher();
int getShmCacheSize();
unique_id_t get_unique_id();
std::string get_arg_session_id();
session_status_t get_session_status();

int getAppArgs(int *argc, char ***argv);

#endif
