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

#if !defined(JOBTASK_H_)
#define JOBTASK_H_

#include <assert.h>
#include "launcher.h"

class JobTask
{
private:
   enum {
      jt_none,
      jt_init,
      jt_launch,
      jt_jobdone,
      jt_sessiondone,
      jt_daemondone
   } task;
   int app_argc;
   char **app_argv;
   app_id_t appid;
   int returncode;
   bool noclean;
public:
   JobTask() :
      task(jt_none),
      app_argc(0),
      app_argv(NULL),
      appid(0),
      returncode(0),
      noclean(false)
   {
   }

   ~JobTask() {
      if (app_argc & !noclean) {
         for (int i = 0; i < app_argc; i++)
            free(app_argv[i]);
         free(app_argv);
      }      
   }

   void setInit()
   {
      task = jt_init;
   }
   
   void setLaunch(int appid_, int app_argc_, char **app_argv_) {
      task = jt_launch;
      app_argc = app_argc_;
      app_argv = app_argv_;
      appid = appid_;
   }

   void setJobDone(app_id_t id, int rc) {
      task = jt_jobdone;
      appid = id;
      returncode = rc;
   }

   void setSessionShutdown() {
      task = jt_sessiondone;
   }
   
   void setDaemonDone(int rc) {
      task = jt_daemondone;
      returncode = rc;
   }

   bool isInit() {
      return task == jt_init;
   }
   
   bool isLaunch() {
      return (task == jt_launch);
   }

   bool isJobDone() {
      return task == jt_jobdone;
   }

   app_id_t getJobDoneID() {
      return appid;
   }

   app_id_t getReturnCode() {
      return returncode;
   }

   bool isSessionShutdown() {
      return task == jt_sessiondone;
   }
   
   bool isDaemonDone() {
      return task == jt_daemondone;
   }

   void getAppArgs(app_id_t &appid_, int &app_argc_, char** &app_argv_) {
      assert(task == jt_launch);
      app_argc_ = app_argc;
      app_argv_ = app_argv;
      appid_ = appid;
   }

   void setNoClean() {
      noclean = true;
   }

   const char *task_str() {
      switch (task) {
         case jt_none: return "none";
         case jt_init: return "init";
         case jt_launch: return "launch";
         case jt_jobdone: return "jobdone";
         case jt_sessiondone: return "sessiondone";
         case jt_daemondone: return "daemon_done";
      }
      return "<Unknown Task ID>";
   }        
};

#endif
