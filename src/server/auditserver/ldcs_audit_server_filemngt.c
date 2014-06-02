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
#include <sys/inotify.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "ldcs_api.h"
#include "ldcs_cache.h"
#include "ldcs_api_listen.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_filemngt.h"
#include "config.h"

char *_ldcs_audit_server_tmpdir;

extern int spindle_mkdir(char *path);

int ldcs_audit_server_filemngt_init (char* location) {
   int rc=0;

   _ldcs_audit_server_tmpdir = location;

   if (-1 == spindle_mkdir(_ldcs_audit_server_tmpdir)) {
      err_printf("mkdir: ERROR during mkdir %s\n", _ldcs_audit_server_tmpdir);
      _error("mkdir failed");
   }

   return(rc);
}

extern int read_file_and_strip(FILE *f, void *data, size_t *size);

char *filemngt_calc_localname(char *global_name)
{
   static unsigned int unique_str_num = 0;
   char target[MAX_NAME_LEN+1];
   char *s, *newname;
   int global_name_len = strlen(global_name);
   int space_used = 0, newname_size = 0;
   target[MAX_NAME_LEN] = '\0';
   
   space_used += snprintf(target, MAX_NAME_LEN, "%x-", unique_str_num);
   unique_str_num++;
   
   if (global_name_len + space_used <= MAX_NAME_LEN) {
      strncpy(target + space_used, global_name, MAX_NAME_LEN - space_used);
   }
   else {
      strncpy(target + space_used,
              global_name + (global_name_len - MAX_NAME_LEN) + space_used,
              MAX_NAME_LEN - space_used);
   }
   for (s = target; *s; s++) {
      if (*s == '/')
         *s = '_';
      if (*s == '*') 
         *s = '_';
   }

   newname_size = strlen(target) + strlen(_ldcs_audit_server_tmpdir) + 2;
   newname = (char *) malloc(newname_size);
   snprintf(newname, newname_size, "%s/%s", _ldcs_audit_server_tmpdir, target);

   return newname;
}

int filemngt_read_file(char *filename, void *buffer, size_t *size, int strip)
{
   FILE *f;
   int result = 0;

   debug_printf2("Reading file %s from disk\n", filename);

   f = fopen(filename, "r");
   if (!f) {
      err_printf("Failed to open file %s\n", filename);
      return -1;
   }

   if (strip) {
      result = read_file_and_strip(f, buffer, size);
   }
   else {
      do {
         result = fread(buffer, 1, *size, f);
      } while (result == -1 && errno == EINTR);
      result = (result == *size) ? 0 : -1;
   }
   if (result == -1)
      err_printf("Error reading from file %s: %s\n", filename, strerror(errno));

   fclose(f);
   return result;
}

int filemngt_encode_packet(char *filename, void *filecontents, size_t filesize, 
                           char **buffer, size_t *buffer_size)
{
   int cur_pos = 0;
   int filename_len = strlen(filename) + 1;
   *buffer_size = filename_len + sizeof(filename_len) + sizeof(filesize) + filesize;
   *buffer = (char *) malloc(*buffer_size);
   if (!*buffer) {
      err_printf("Failed to allocate memory for file contents packet for %s\n", filename);
      return -1;
   }
   
   memcpy(*buffer + cur_pos, &filename_len, sizeof(filename_len));
   cur_pos += sizeof(filename_len);
   
   memcpy(*buffer + cur_pos, &filesize, sizeof(filesize));
   cur_pos += sizeof(filesize);

   memcpy(*buffer + cur_pos, filename, filename_len);
   cur_pos += filename_len;

   /* Explicitely removing the memcpy that puts the file contents into the
      packet.  In order to keep file contents zero-copy we won't add them
      to the packet, but will instead send them with a second write command.
   memcpy(*buffer + cur_pos, filecontents, filesize);
   */
   cur_pos += filesize;

   assert(cur_pos == *buffer_size);
   return 0;
}

int filemngt_decode_packet(node_peer_t peer, ldcs_message_t *msg, char *filename, size_t *filesize)
{
   /* We've delayed the file read from the network.  Just read the filename and size here.
      We'll later get the file contents latter by reading directly to mapped memory */
   int filename_len = 0;
   int result;
   
   result = ldcs_audit_server_md_complete_msg_read(peer, msg, &filename_len, sizeof(filename_len));
   if (result == -1)
      return -1;
   assert(filename_len > 0 && filename_len <= MAX_PATH_LEN+1);

   result = ldcs_audit_server_md_complete_msg_read(peer, msg, filesize, sizeof(*filesize));
   if (result == -1)
      return -1;

   result = ldcs_audit_server_md_complete_msg_read(peer, msg, filename, filename_len);
   if (result == -1)
      return -1;

   return 0;
}

/**
 * Clear files from the local ramdisk
 **/
int ldcs_audit_server_filemngt_clean()
{
#if !defined(USE_CLEANUP_PROC)
   DIR *tmpdir;
   struct dirent *dp;
   char path[MAX_PATH_LEN];
   struct stat finfo;
   int result;

   debug_printf("Cleaning tmpdir %s\n", _ldcs_audit_server_tmpdir);

   tmpdir = opendir(_ldcs_audit_server_tmpdir);
   if (!tmpdir) {
      err_printf("Failed to open %s for cleaning: %s\n", _ldcs_audit_server_tmpdir,
                 strerror(errno));
      return -1;
   }
   
   while ((dp = readdir(tmpdir))) {
      snprintf(path, MAX_PATH_LEN, "%s/%s", _ldcs_audit_server_tmpdir, dp->d_name);
      result = lstat(path, &finfo);
      if (result == -1) {
         err_printf("Failed to stat %s\n", path);
         continue;
      }
      if (!S_ISREG(finfo.st_mode)) {
         debug_printf3("Not cleaning file %s\n", path);
         continue;
      }
      unlink(path);
   }

   closedir(tmpdir);
   rmdir(_ldcs_audit_server_tmpdir);
#endif
   return 0;
}

int filemngt_create_file_space(char *filename, size_t size, void **buffer_out, int *fd_out)
{
   int result;
   *fd_out = open(filename, O_CREAT | O_EXCL | O_RDWR, 0700);
   if (*fd_out == -1) {
      err_printf("Could not create local file %s: %s\n", filename, strerror(errno));
      return -1;
   }
   if (size == 0) {
       size = getpagesize();
       debug_printf2("growing empty file to size %d", (int) size);
   }
   result = ftruncate(*fd_out, size);
   if (result == -1) {
      err_printf("Could not grow local file %s to %lu (out of memory?): %s\n", filename, size, strerror(errno));
      close(*fd_out);
      return -1;
   }
   *buffer_out = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd_out, 0);
   if (*buffer_out == MAP_FAILED) {
      err_printf("Could not mmap file %s: %s\n", filename, strerror(errno));
      close(*fd_out);
      return -1;
   }
   assert(*buffer_out);
   return 0;
}

void *filemngt_sync_file_space(void *buffer, int fd, char *pathname, size_t size, size_t newsize)
{
   /* Linux gets annoying here.  We can't just mprotect the buffer to read-only,
      Linux considers the file still open for writing (and thus throws ETXTBUSY on exec's)
      as long as we still have the buffer mmaped from a fd that was opened with write.
      That means we've got to close the file, unmap the buffer, then re-open the file 
      read-only, then re-map the buffer to the same place.  Ugh.  
   */

   int result;
   char *buffer2;

   if (size == 0) {
       newsize = size = getpagesize();
       debug_printf2("growing empty file to size %d", (int) size);
   }

   result = munmap(buffer, size);
   if (result == -1) {
      err_printf("Error unmapping buffer for %s\n", pathname);
      return NULL;
   }

   if (size != newsize) {
      assert(newsize < size);
      /* The file shrunk after we read it, probably because we stripped it
         as we read.  Shrink the local file on disk and shrink the mapped region */
      result = ftruncate(fd, newsize);
      if (result == -1) {
         err_printf("Could not shrink file in local disk\n");
         return NULL;
      }
   }
   
   close(fd);
   
   fd = open(pathname, O_RDONLY);
   if (fd == -1) {
      err_printf("Failed to re-open file %s: %s\n", pathname, strerror(errno));
      return NULL;
   }
   
   buffer2 = mmap(buffer, newsize, PROT_READ, MAP_SHARED | MAP_FIXED, fd, 0);
   if (buffer2 == MAP_FAILED) {
      debug_printf("Failure re-mapping file %s: %s. Retrying.\n", pathname, strerror(errno));
      buffer2 = mmap(NULL, newsize, PROT_READ, MAP_SHARED, fd, 0);
      if (buffer2 == MAP_FAILED) {
         debug_printf("Could not re-mapping file %s: %s\n", pathname, strerror(errno));
         return NULL;
      }
   }

   close(fd);

   return buffer2;
}

size_t filemngt_get_file_size(char *pathname)
{
   struct stat st;
   int result;

   result = stat(pathname, &st);
   if (result == -1) {
      err_printf("Error stat'ing %s, which should exist\n", pathname);
      return (size_t) -1;
   }
   return (size_t) st.st_size;
}

int filemngt_stat(char *pathname, struct stat *buf)
{
   int result;
   if (*pathname == '*') {
      result = stat(pathname+1, buf);
      debug_printf3("stat(%s) = %d\n", pathname, result);
   }
   else {
      result = lstat(pathname, buf);
      debug_printf3("lstat(%s) = %d\n", pathname, result);
   }
   return result;
}

int filemngt_write_stat(char *localname, struct stat *buf)
{
   int result, bytes_written, fd;
   int size;
   char *buffer;

   fd = creat(localname, 0600);
   if (fd == -1) {
      err_printf("Failed to create file %s for writing: %s\n", localname, strerror(errno));
      return -1;
   }

   bytes_written = 0;
   buffer = (char *) buf;
   size = sizeof(struct stat);

   while (bytes_written != size) {
      result = write(fd, buffer + bytes_written, size - bytes_written);
      if (result <= 0) {
         if (errno == EAGAIN || errno == EINTR)
            continue;
         err_printf("Failed to write to file %s: %s\n", localname, strerror(errno));
         close(fd);
         return -1;
      }
      bytes_written += result;
   }
   close(fd);
   return 0;
}

int filemngt_read_stat(char *localname, struct stat *buf)
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
