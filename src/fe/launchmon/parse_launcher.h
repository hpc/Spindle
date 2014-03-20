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

#if !defined(parse_launcher_h_)
#define parse_launcher_h_

#include <set>
#include <string>
#include <vector>
#include <map>
#include "spindle_launch.h"

class CmdLineParser;
struct cmdoption_t;

/**
 * External command for parsing launcher command lines
 **/
class ModifyArgv {
  private:
   int argc;
   char **argv;
   int new_argc;
   char **new_argv;
   spindle_args_t *params;
   CmdLineParser *parser;
   
   void exit_w_err(std::string str);
   void autodetectParser();
   void chooseParser();
   void modifyCmdLine();
   
  public:
   ModifyArgv(int argc, char **argv,
              spindle_args_t *params);
   void getNewArgv(int &newargc, char** &newargv);
};

/**
 * LauncherParser is instanciated once for every type of launcher (slurm, openmpi, mvapich, ...)
 * It can parse a command line for each type of launcher.
 **/
class LauncherParser {
protected:
   std::set<std::string> launcher_cmds;
   std::map<std::string, cmdoption_t*> arg_list;
   std::string bg_addenv_str;
   std::string name;
   int code;
   bool isIntegerString(std::string str) const;
public:
   LauncherParser(cmdoption_t *options, size_t options_size, std::string bg_string, std::string name_, int code_);
   virtual ~LauncherParser();

   virtual bool valid(int argc, char **argv);
   virtual bool valid2(int argc, char **argv);
   virtual bool usesLauncher() const;
   virtual bool isLauncher(int argc, char **argv, int pos) const;
   virtual bool isExecutable(int argc, char **argv, int pos, const std::set<std::string> &exedirs) const;
   virtual bool parseCustomArg(int argc, char **argv, int arg_pos, int &inc_argc) const;
   cmdoption_t *getArg(int argc, char **argv, int pos) const;
   bool getArgOptions(int argc, char **argv, int arg_pos, cmdoption_t *opt,
                      std::vector<std::string> &argOptions, int &inc_argc) const;
   std::string getName() const;
   std::string getBGString() const;
   int getCode() const;
};

/**
 * Types for instances of launchers
 **/
typedef LauncherParser SRunParser;

class SerialParser : public LauncherParser
{
  public:
   SerialParser(cmdoption_t *options, size_t options_size, std::string bg_string, std::string name_, int code_);
   virtual ~SerialParser();

   virtual bool usesLauncher() const;
   virtual bool isExecutable(int argc, char **argv, int pos, const std::set<std::string> &exedirs) const;
};

class OpenMPIParser : public LauncherParser
{
  public:
   OpenMPIParser(cmdoption_t *options, size_t options_size, std::string bg_string, std::string name_, int code_);
   virtual ~OpenMPIParser();

   virtual bool parseCustomArg(int argc, char **argv, int arg_pos, int &inc_argc) const;
};

typedef LauncherParser WreckRunParser;

extern SRunParser *srunparser;
extern SerialParser *serialparser;
extern OpenMPIParser *openmpiparser;
extern WreckRunParser *wreckrunparser;

void initParsers(int parsers_enabled, std::set<LauncherParser *> &all_parsers);

/**
 * A CmdLineParser is an instance of a argv/argc command line that will
 * be built with a specific LauncherParser.  
 **/
class CmdLineParser
{
private:
   int argc;
   char **argv;
   int launcher_at;
   int exec_at;
   LauncherParser *parser;
   std::set<std::string> exedirs;
public:
   CmdLineParser(int argc_, char **argv_, LauncherParser *parser_);
   ~CmdLineParser();

   enum parse_ret_t {
      success,
      no_exec,
      no_launcher
   };
   parse_ret_t parse();

   int launcherAt();
   int appExecutableAt();
   LauncherParser *getParser();
};

/**
 * A singleton for testing whether a file is an executable
 **/
class ExeTest {
private:
   std::vector<std::string> path;
   bool isExec(std::string executable, std::string directory = std::string());
   bool isPathExec(std::string executable);
public:
   ExeTest();
   bool isExecutableFile(std::string file, const std::set<std::string> &exedirs);
};


/**
 * A launcher's command line is described by the fl_options flags and the 
 * 
 **/
struct cmdoption_t {
   const char *opt;
   const char *long_opt;
   int flags; //Bitmask of FL_* values
};

//FL_LAUNCHER defines common names for the mpi launcher (mpiexec, mpirun, srun, ...)
#define FL_LAUNCHER       1<<0
//FL_GNU_PARAM is a parameter following a GNU-style.  (E.g, --arg=val or -a val)
#define FL_GNU_PARAM      1<<1
//FL_PARAM and FL_PARAM2 are arguments that respecitevly take 1 or 2 parameters as subsequent args (e.g, -n 5)
#define FL_PARAM          1<<2
#define FL_PARAM2         1<<3
//FL_OPTIONAL is set if the parameter to an arguement is optional
#define FL_OPTIONAL       1<<4
//FL_INTEGER is set if the parameter to an argument can only be an integer
#define FL_INTEGER        1<<5
//FL_EXEDIR et if the parameter to an argument specifies a directory that should be checked when searching
// for the executable
#define FL_EXEDIR         1<<6
//FL_OPTIONAL_DASH is set if the argument can appear with 1 or 2 dashes.
#define FL_OPTIONAL_DASH  1<<7
//FL_CUSTOM_OPTION specified that an option is handled by a launcher's custom handler
#define FL_CUSTOM_OPTION  1<<8

#endif
