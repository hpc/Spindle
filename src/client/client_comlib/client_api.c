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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include "ldcs_api.h"
#include "client_api.h"
#include "client_heap.h"

static struct lock_t comm_lock;

#define COMM_LOCK do { if (lock(&comm_lock) == -1) return -1; } while (0)
#define COMM_UNLOCK unlock(&comm_lock)
   
int send_file_query(int fd, char* path, char** newpath, int *errcode) {
   ldcs_message_t message;
   char buffer[MAX_PATH_LEN+1+sizeof(int)];
   int result;
   int path_len = strlen(path)+1;
   buffer[MAX_PATH_LEN+sizeof(int)] = '\0';
    
   if (path_len > MAX_PATH_LEN) {
      err_printf("Path to long for message");
      return -1;
   }

   /* Setup packet */
   message.header.type = LDCS_MSG_FILE_QUERY_EXACT_PATH;
   message.header.len = path_len;
   message.data = buffer;
   strncpy(message.data, path, MAX_PATH_LEN);

   COMM_LOCK;

   debug_printf3("sending message of type: file_query len=%d data='%s' ...(%s)\n",
                 message.header.len, message.data, path);  
   client_send_msg(fd, &message);

   /* get new filename */
   client_recv_msg_static(fd, &message, LDCS_READ_BLOCK);

   COMM_UNLOCK;

   if (message.header.type != LDCS_MSG_FILE_QUERY_ANSWER) {
      err_printf("Got unexpected message of type %d\n", (int) message.header.type);
      assert(0);
   }
   
   if (message.header.len > sizeof(int)) {
      *newpath = spindle_strdup(message.data + sizeof(int));
      *errcode = 0;
      result = 0;
   } 
   else {
      *errcode = *((int *) message.data);
      *newpath = NULL;
      result = 0;
   }

   return result;
}

int send_stat_request(int fd, char *path, int is_lstat, char *newpath)
{
   int path_len = strlen(path) + (is_lstat ? 0 : 1) + 1;
   ldcs_message_t message;

   if (path_len >= MAX_PATH_LEN+1) {
      err_printf("stat path of %s is too long for Spindle\n", path);
      return -1;
   }

   strncpy(newpath, path, MAX_PATH_LEN+1);
   newpath[MAX_PATH_LEN] = '\0';
   
   /* Setup packet */
   message.header.type = is_lstat ? LDCS_MSG_LSTAT_QUERY : LDCS_MSG_STAT_QUERY;
   message.header.len = path_len;
   message.data = newpath;
   
   COMM_LOCK;

   debug_printf3("sending message of type: %sstat_query len=%d data='%s' ...(%s)\n",
                 is_lstat ? "l" : "", message.header.len, message.data, path);  
   client_send_msg(fd, &message);

   /* get new filename */
   client_recv_msg_static(fd, &message, LDCS_READ_BLOCK);

   COMM_UNLOCK;
      
   if (message.header.type != LDCS_MSG_STAT_ANSWER) {
      err_printf("Got unexpected message of type %d\n", message.header.type);
      return -1;
   }
   if (message.header.len == 0) {
      debug_printf3("stat of file %s says file doesn't exist\n", path);
      *newpath = '\0';
      return 0;
   }

   return 0;
}

int send_existance_test(int fd, char *path, int *exists)
{
   ldcs_message_t message;
   char buffer[MAX_PATH_LEN+1];
   buffer[MAX_PATH_LEN] = '\0';
   int path_len = strlen(path)+1;
    
   if (path_len > MAX_PATH_LEN) {
      err_printf("Path to long for message");
      return -1;
   }
   strncpy(buffer, path, MAX_PATH_LEN);

   message.header.type = LDCS_MSG_EXISTS_QUERY;
   message.header.len = strlen(path) + 1;
   message.data = (void *) buffer;

   debug_printf3("Sending message of type: file_exist_query len=%d, data=%s\n",
                 message.header.len, path);
   COMM_LOCK;

   client_send_msg(fd, &message);

   client_recv_msg_static(fd, &message, LDCS_READ_BLOCK);

   COMM_UNLOCK;

   if (message.header.type != LDCS_MSG_EXISTS_ANSWER || message.header.len != sizeof(uint32_t)) {
      err_printf("Got unexpected message after existance test: %d\n", (int) message.header.type);
      assert(0);
   }

   memcpy(exists, buffer, sizeof(*exists));
   debug_printf3("Determined that file %s %s\n", path, *exists ? "exists" : "does not exist");
   return 0;
}

int send_orig_path_request(int fd, const char *path, char *newpath)
{
   ldcs_message_t message;
   char buffer[MAX_PATH_LEN+1];
   buffer[MAX_PATH_LEN] = '\0';
   int path_len = strlen(path)+1;
    
   if (path_len > MAX_PATH_LEN) {
      err_printf("Path to long for message");
      return -1;
   }
   strncpy(buffer, path, MAX_PATH_LEN);

   message.header.type = LDCS_MSG_ORIGPATH_QUERY;
   message.header.len = strlen(path) + 1;
   message.data = (void *) buffer;

   debug_printf3("Sending message of type: file_orig_path len=%d, data=%s\n",
                 message.header.len, path);
   COMM_LOCK;

   client_send_msg(fd, &message);

   client_recv_msg_static(fd, &message, LDCS_READ_BLOCK);

   COMM_UNLOCK;

   if (message.header.type != LDCS_MSG_ORIGPATH_ANSWER || message.header.len > MAX_PATH_LEN) {
      err_printf("Got unexpected message after existance test: %d\n", (int) message.header.type);
      assert(0);
   }
   strncpy(newpath, buffer, MAX_PATH_LEN+1);

   return 0;
}

int send_dir_cwd(int fd, char *cwd)
{
   ldcs_message_t message;

   message.header.type = LDCS_MSG_CWD;
   message.header.len = strlen(cwd) + 1;
   message.data = cwd;

   COMM_LOCK;

   client_send_msg(fd, &message);

   COMM_UNLOCK;

   return 0;
}

int send_cwd(int fd)
{
   char buffer[MAX_PATH_LEN+1];
   buffer[MAX_PATH_LEN] = '\0';

   if (!getcwd(buffer, MAX_PATH_LEN)) {
      return -1;
   }

   send_dir_cwd(fd, buffer);

   return 0;
}

int send_pid(int fd) {
   ldcs_message_t message;
   char buffer[16];
   int rc=0;

   snprintf(buffer, 16, "%d", getpid());
   message.header.type = LDCS_MSG_PID;
   message.header.len = 16;
   message.data = buffer;

   debug_printf3("Sending pid\n");

   COMM_LOCK;

   client_send_msg(fd,&message);

   COMM_UNLOCK;

   return(rc);
}

int send_cpu(int fd, int cpu) {
   ldcs_message_t message;
   char buffer[16];
   int rc=0;

   snprintf(buffer, 16, "%d", cpu);
   message.header.type = LDCS_MSG_CPU;
   message.header.len = 16;
   message.data = buffer;

   debug_printf3("Sending cpu %d\n", cpu);

   COMM_LOCK;

   client_send_msg(fd, &message);

   COMM_UNLOCK;

   return(rc);
}

int send_location(int fd, char *location) {
   ldcs_message_t message;

   message.header.type = LDCS_MSG_LOCATION;
   message.header.len = strlen(location)+1;
   message.data = location;

   debug_printf3("Sending location\n");

   COMM_LOCK;

   client_send_msg(fd,&message);

   COMM_UNLOCK;

   return 0;
}

int send_ldso_info_request(int fd, const char *ldso_path, char *result_path)
{
   ldcs_message_t message;

   message.header.type = LDCS_MSG_LOADER_DATA_REQ;
   message.header.len = strlen(ldso_path)+2;
   message.data = (void *) ldso_path;
   
   COMM_LOCK;
   client_send_msg(fd, &message);
   message.data = result_path;
   client_recv_msg_static(fd, &message, LDCS_READ_BLOCK);
   COMM_UNLOCK;

   if (message.header.type != LDCS_MSG_LOADER_DATA_RESP) {
      err_printf("Got unexpected message after ldso req: %d\n", (int) message.header.type);
      assert(0);
   }
   return 0;
}

int get_python_prefix(int fd, char **prefix)
{
   ldcs_message_t message;
   message.header.type = LDCS_MSG_PYTHONPREFIX_REQ;
   message.header.len = 0;
   message.data = NULL;
      
   COMM_LOCK;
   client_send_msg(fd, &message);
   client_recv_msg_dynamic(fd, &message, LDCS_READ_BLOCK);
   COMM_UNLOCK;
   *prefix = (char *) message.data;
   return 0;
}

int send_rankinfo_query(int fd, int *mylrank, int *mylsize, int *mymdrank, int *mymdsize) {
   ldcs_message_t message;
   char buffer[MAX_PATH_LEN];
   int *p;

   debug_printf3("Sending rankinfo query\n");

   message.header.type=LDCS_MSG_MYRANKINFO_QUERY;
   message.header.len=0;
   message.data=buffer;

   COMM_LOCK;

   client_send_msg(fd,&message);

   client_recv_msg_static(fd, &message, LDCS_READ_BLOCK);

   COMM_UNLOCK;

   if (message.header.type != LDCS_MSG_MYRANKINFO_QUERY_ANSWER || message.header.len != 4*sizeof(int)) {
      err_printf("Received incorrect response to rankinfo query\n");
      *mylrank = *mylsize = *mymdrank = *mymdsize = -1;
      assert(0);
   }

   p = (int *) message.data;
   *mylrank = p[0];
   *mylsize = p[1];
   *mymdrank = p[2];
   *mymdsize = p[3];
   debug_printf3("received rank info: local: %d of %d md: %d of %d\n", *mylrank, *mylsize, *mymdrank, *mymdsize);

   return 0;
}

int send_end(int fd) {
   ldcs_message_t message;
   
   message.header.type = LDCS_MSG_END;
   message.header.len = 0;
   message.data = NULL;
   
   COMM_LOCK;
   
   client_send_msg(fd, &message);
   
   COMM_UNLOCK;
   
   return 0;
}

