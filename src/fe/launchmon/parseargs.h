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

#if !defined(PARSEARGS_H_)
#define PARSEARGS_H_

#include <string>

unsigned long parseArgs(int argc, char *argv[]);
char *getPreloadFile();
unsigned int getPort();
std::string getLocation(int number);
std::string getPythonPrefixes();
bool isLoggingEnabled();
int getLauncher();
bool hideFDs();
int getAppArgs(int *argc, char ***argv);

#endif
