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
#include "ldcs_api_opts.h"
#include "client_api.h"
#include "client_heap.h"

static struct lock_t comm_lock;

#define COMM_LOCK do { if (lock(&comm_lock) == -1) return -1; } while (0)
#define COMM_UNLOCK unlock(&comm_lock)
   
int send_file_query(int fd, char* path, char** newpath) {
   ldcs_message_t message;
   char buffer[MAX_PATH_LEN+1];
   buffer[MAX_PATH_LEN] = '\0';
   int result;
   int path_len = strlen(path)+1;
    
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
   
   if (message.header.len > 0) {
      *newpath = spindle_strdup(message.data);
      result = 0;
   } 
   else {
      *newpath = NULL;
      result = -1;
   }

   return result;
}

static int read_stat(char *localname, struct stat *buf)
{
   int result, bytes_read, fd;
   int size;
   char *buffer;

   fd = open(localname, O_RDONLY);
   if (fd == -1) {
      err_printf("Failed to open %s for reading: %s\n", localname, strerror(errno));
      return -1;
   }

   bytes_read = 0;
   buffer = (char *) buf;
   size = sizeof(struct stat);

   while (bytes_read != size) {
      result = read(fd, buffer + bytes_read, size - bytes_read);
      if (result <= 0) {
         if (errno == EAGAIN || errno == EINTR)
            continue;
         err_printf("Failed to read from file %s: %s\n", localname, strerror(errno));
         close(fd);
         return -1;
      }
      bytes_read += result;
   }
   close(fd);
   return 0;
}

int send_stat_request(int fd, char *path, int is_lstat, int *exists, struct stat *buf)
{
   char buffer[MAX_PATH_LEN+1];
   int path_len = strlen(path) + (is_lstat ? 0 : 1) + 1;
   int result;
   char *localname;
   ldcs_message_t message;

   buffer[MAX_PATH_LEN] = '\0';

   if (path_len >= MAX_PATH_LEN+1) {
      err_printf("stat path of %s is too long for Spindle\n", path);
      *exists = 0;
      return -1;
   }

   /* Regular stats are sent with a preceeding '*', lstats are not */
   snprintf(buffer, MAX_PATH_LEN+1, "%s%s", is_lstat ? "" : "*", path);
   
   /* Setup packet */
   message.header.type = LDCS_MSG_STAT_QUERY;
   message.header.len = path_len;
   message.data = buffer;
   
   COMM_LOCK;

   debug_printf3("sending message of type: stat_query len=%d data='%s' ...(%s)\n",
                 message.header.len, message.data, path);  
   client_send_msg(fd, &message);

   /* get new filename */
   client_recv_msg_static(fd, &message, LDCS_READ_BLOCK);

   COMM_UNLOCK;
      
   if (message.header.type != LDCS_MSG_STAT_ANSWER) {
      err_printf("Got unexpected message of type %d\n", message.header.type);
      assert(0);
   }
   if (message.header.len == 0) {
      debug_printf3("stat of file %s says file doesn't exist\n", path);
      *exists = 0;
      return 0;
   }

   localname = (char *) message.data;
   result = read_stat(localname, buf);
   if (result == -1) {
      err_printf("Failed to read stat info for %s from %s\n", path, localname);
      *exists = 0;
      return -1;
   }

   *exists = 1;
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

   *exists = (int) *((uint32_t *) buffer);
   debug_printf3("Determined that file %s %s\n", path, *exists ? "exists" : "does not exist");
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


   COMM_LOCK;

   client_send_msg(fd,&message);

   COMM_UNLOCK;

   return(rc);
}

int send_location(int fd, char *location) {
   ldcs_message_t message;

   message.header.type = LDCS_MSG_LOCATION;
   message.header.len = strlen(location)+1;
   message.data = location;

   COMM_LOCK;

   client_send_msg(fd,&message);

   COMM_UNLOCK;

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

