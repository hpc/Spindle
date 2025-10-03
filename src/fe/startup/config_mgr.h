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

#if !defined(CONFIG_MGR_H_)
#define CONFIG_MGR_H_

#include <map>
#include <string>
#include <utility>
#include <set>
#include <list>
#include "spindle_launch.h"

enum SpindleConfigID {
   confCmdlineOnly,
   confCmdlineNewgroup,
   confPort,
   confNumPorts,
   confLocation,
   confCachePaths,
   confCachePrefix,
   confPythonPrefix,
   confLocalPrefix,
   confStrip,
   confRelocAout,
   confRelocLibs,
   confRelocPython,
   confRelocExec,
   confFollowFork,
   confStopReloc,
   confSpindleLevel,
   confPushpull,
   confNetwork,
   confSecurity,
   confJoblauncher,
   confNuma,
   confNumaIncludes,
   confNumaExcludes,
   confAuditType,
   confShmcacheSize,
   confDebug,
   confPreload,
   confHostbin,
   confNoclean,
   confDisableLogging,
   confNoHide,
   confPersist,
   confMsgcacheBuffer,
   confMsgcacheTimeout,
   confCleanupProc,
   confExecExcludes,
   confEnableRsh,
   confRshCommand,
   confStartSession,
   confStartMultiSession,
   confEndSession,
   confRunSession,
   confPatchLdso
};

enum CmdlineShortOptions {
   shortNone = 0,
   shortRelocAout = 'a',
   shortSharedCacheSize = 'b',
   shortCobo = 'c',
   shortDebug = 'd',
   shortPreload = 'e',
   shortFollowFork = 'f',
   shortHide = 'h',
   shortAuditType = 'k',
   shortRelocSO = 'l',
   shortNoClean = 'n',
   shortLocation = 'o',
   shortPush = 'p',
   shortPull = 'q',
   shortPythonPrefix = 'r',
   shortStrip = 's',
   shortPort = 't',
   shortNUMA = 'u',
   shortStopReloc = 'w',
   shortRelocExec = 'x',
   shortRelocPy = 'y',
   shortDisableLogging = 'z',
   shortSecMunge = (256+OPT_SEC_MUNGE),
   shortSecKeyLMon = (256+OPT_SEC_KEYLMON),
   shortSecKeyFile = (256+OPT_SEC_KEYFILE),
   shortSecNull = (256+OPT_SEC_NULL),
   shortOpenMPI = 271,
   shortWreck = 272,
   shortNoMPI = 273,
   shortHostbin = 274,
   shortPersist = 275,
   shortStartSession = 276,
   shortStartMultiSession = 277,
   shortRunSession = 278,
   shortEndSession = 279,
   shortLauncherStartup = 280,
   shortMsgcacheBuffer = 281,
   shortMsgcacheTimeout = 282,
   shortCleanupProc = 283,
   shortRSHMode = 284,
   shortNUMAIncludes = 285,
   shortNUMAExcludes = 286,
   shortCachePrefix = 287,
   shortNumPorts = 288,
   shortSerial = 289,
   shortRSHCmd = 290,
   shortPushPull = 291,
   shortSecuritySet = 292,
   shortLauncher = 293,
   shortNetwork = 294,
   shortHostbinEnable = 295,
   shortSpindleLevel = 296,
   shortLocalPrefix = 297,
   shortExecExcludes = 298,
   shortPatchLdso,
   shortCachePaths,
};

enum CmdlineGroups {
   groupReloc = 1,
   groupPushPull = 2,
   groupNetwork = 3,
   groupSec = 4,
   groupLauncher = 5,
   groupSession = 6,
   groupNuma = 7,
   groupMisc = 8
};

enum ConfigValueType {
   cvBool,
   cvInteger,
   cvString,
   cvStringOptional,
   cvList,
   cvEnum
};

struct SpindleOption {
   SpindleConfigID config_id;
   const char *cmdline_long;
   CmdlineShortOptions cmdline_key;
   CmdlineGroups cmdline_group;
   ConfigValueType conftype;
   std::list<const char *> enum_values;
   const char *default_value;
   const char *helptext;
};

const std::map<SpindleConfigID, const SpindleOption&> &getOptionsByID();
const std::map<CmdlineShortOptions, const SpindleOption&> &getOptionsByKey();
const std::map<std::string, const SpindleOption&> &getOptionsByLongName();

extern const std::list<SpindleOption> *Options;

void initOptionsList();

typedef enum {
   sstatus_unused,
   sstatus_start,
   sstatus_run,
   sstatus_end
} session_status_t;

class ConfigMap
{
  private:
   std::map<SpindleConfigID, std::string> name_values;
   std::string origin;
   std::list<std::string> app_commandline;
   mutable unique_id_t unique_id_saved;
   mutable number_t number_saved;
  public:
   ConfigMap();
   ConfigMap(std::string origin_);
   std::string mergeLists(std::string value, std::string existing);
   bool mergeOnto(const ConfigMap &other);
   bool set(SpindleConfigID name, std::string value, std::string &errstring);
   void debugPrint() const;
   void setOrigin(std::string s);
   void setApplicationCmdline(int argc, char **argv);
   
   std::pair<bool, std::string> getValueString(SpindleConfigID name) const;
   std::pair<bool, long> getValueIntegral(SpindleConfigID name) const;
   std::pair<bool, bool> getValueBool(SpindleConfigID name) const;
   std::pair<bool, std::string> getValueEnum(SpindleConfigID name) const;
   const std::list<std::string> &getApplicationCmdline() const { return app_commandline; }
   bool getApplicationCmdline(int &argc, char** &argv) const;
   bool isSet(SpindleConfigID name) const;
   bool toSpindleArgs(spindle_args_t &args, bool alloc_strs = false) const;

   session_status_t getSessionStatus() const;
   std::string getArgSessionId() const;

   bool getUniqueID(unique_id_t &unique_id, std::string &errmsg) const;
   number_t getNumber() const;
};

bool gatherAllConfigInfo(int argc, char *argv[], bool inExecutable, ConfigMap &confmap, std::string &errmessage);
std::pair<bool, bool> strToBool(std::string s);
int parseArgs(int argc, char *argv[], bool inExecutable, std::string &errmsg, ConfigMap &confresult);

#endif
