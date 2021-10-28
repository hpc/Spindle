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

#define _DEFAULT_SOURCE

#include "parse_mounts.h"

#include <sys/types.h>
#include <sys/sysmacros.h>

#include <map>
#include <utility>
#include <string>
#include <stdio.h>
#include <cstring>


#define MAX_PATH_LEN 4096
using namespace std;

typedef pair<int, int> device_t;

static map<device_t, string> device_to_mountpoint;
static map<string, device_t> mountpoint_to_device;

static bool parse()
{
   static bool mounts_parsed = false;
   static bool mounts_parsed_result = false;

   if (mounts_parsed)
      return mounts_parsed_result;   
   mounts_parsed = true;

   int result;
   FILE *f = fopen("/proc/self/mountinfo", "r");
   if (!f) {
      mounts_parsed_result = false;
      return mounts_parsed_result;
   }

   while (!feof(f)) {
      int major, minor;
      char mount_point[MAX_PATH_LEN+1];
 
      mount_point[MAX_PATH_LEN] = '\0';
      
      fscanf(f, "%*d %*d %d:%d %*s %4096s", &major, &minor, mount_point);
      do {
         result = fgetc(f);
      } while (result != (int) '\n' && result != EOF);

      string mount(mount_point);
      device_t dev = make_pair(major, minor);

      if (device_to_mountpoint.find(dev) == device_to_mountpoint.end())
         device_to_mountpoint.insert(make_pair(dev, mount));
      if (mountpoint_to_device.find(mount) == mountpoint_to_device.end())
         mountpoint_to_device.insert(make_pair(mount, dev));
   }
   fclose(f);

   mounts_parsed_result = true;
   return mounts_parsed_result;
}

int mount_to_dev(const char *mount_str, dev_t *device)
{
   if (!parse())
      return -1;   
   string m(mount_str);
   map<string, device_t>::iterator i = mountpoint_to_device.find(m);
   if (i == mountpoint_to_device.end())
      return -1;
   int major = i->second.first;
   int minor = i->second.second;
   *device = gnu_dev_makedev(major, minor);
   return 0;
}

int dev_to_mount(dev_t device, const char **mount_str)
{
   if (!parse())
      return -1;

   int major = gnu_dev_major(device);
   int minor = gnu_dev_minor(device);

   device_t d(major, minor);
   map<device_t, string>::iterator i = device_to_mountpoint.find(d);
   if (i == device_to_mountpoint.end())
      return -1;
   *mount_str = i->second.c_str();
   return 0;
}
