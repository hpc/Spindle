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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "ldcs_api.h"
#include "ldcs_cache.h"
#include "ldcs_api_listen.h"
#include "ldcs_audit_server_process.h"
#include "ldcs_audit_server_filemngt.h"

 char _ldcs_audit_server_tmpdir[MAX_PATH_LEN];

 long _ldcs_file_read(FILE *infile, void *data, int bytes );
 long _ldcs_file_write(FILE *outfile, const void *data, int bytes );

int ldcs_audit_server_filemngt_init (char* location) {
   int rc=0;
   struct stat st;

   snprintf(_ldcs_audit_server_tmpdir, MAX_PATH_LEN, "%s/spindle.%d", location, getpid());

   if (stat(_ldcs_audit_server_tmpdir, &st) == -1) {
     /* try create directory */
      if (-1 == mkdir(_ldcs_audit_server_tmpdir, 0700)) {
         err_printf("mkdir: ERROR during mkdir %s\n", _ldcs_audit_server_tmpdir);
         _error("mkdir failed");
      }
   } else {
     if(S_ISDIR(st.st_mode)) {
       debug_printf3("%s already exists, using this direcory\n",_ldcs_audit_server_tmpdir);
     } else {
        err_printf("mkdir: ERROR %s exists and is not a directory\n", _ldcs_audit_server_tmpdir);
        _error("mkdir failed");
     }
   }

   return(rc);
}


int read_file_and_strip(FILE *f, void *data, size_t *size);
int ldcs_audit_server_filemngt_read_file ( char *filename, char *dirname, char *fullname, int domangle,
					   ldcs_message_t* msg ) {
  int rc=0;
  FILE *infile;
  char *file_data,*p,*len_pos;
  int filename_len;
  int dirname_len;
  int fullname_len;
  long file_data_len, file_len;

  debug_printf3("read file data for file: '%s' (%s)  \n", filename, fullname);
  infile = fopen(fullname, "rb");
  if (!infile)  _error("Could not open file");
  debug_printf3("  file open successful \n");
  
  fseek(infile, 0, SEEK_END);
  file_len=ftell(infile);
  fseek(infile, 0, SEEK_SET);
  debug_printf3("  file size %ld bytes (%10.4f MB)\n", file_len, file_len/1024.0/1024.0);

  file_data_len=0;
  file_data_len+=sizeof(domangle);

  filename_len=strlen(filename)+1;
  file_data_len+=sizeof(filename_len)+filename_len;

  dirname_len=strlen(dirname)+1;
  file_data_len+=sizeof(dirname_len)+dirname_len;

  fullname_len=strlen(fullname)+1;
  file_data_len+=sizeof(fullname_len)+fullname_len;

  file_data_len+=sizeof(file_len)+file_len;


  file_data=(char *)malloc(file_data_len+1);
  if (!file_data)  _error("could not allocate memory for file data");

  p=file_data;
  memcpy(p,&domangle,sizeof(domangle)); p+=sizeof(domangle);
  memcpy(p,&filename_len,sizeof(filename_len)); p+=sizeof(filename_len);
  memcpy(p,filename,filename_len);p+=filename_len;
  memcpy(p,&dirname_len,sizeof(dirname_len)); p+=sizeof(dirname_len);
  memcpy(p,dirname,dirname_len);p+=dirname_len;
  memcpy(p,&fullname_len,sizeof(fullname_len)); p+=sizeof(fullname_len);
  memcpy(p,fullname,fullname_len);p+=fullname_len;
  len_pos = p; p+=sizeof(file_len);

  read_file_and_strip(infile, p, (size_t *) &file_len);
  p += file_len;
  fclose(infile);
  debug_printf3("  file read and closed %ld bytes\n", file_len);

  memcpy(len_pos, &file_len, sizeof(file_len));
  
  msg->data = file_data;
  msg->header.len = p - file_data;
  msg->alloclen = file_data_len;
  rc = file_len;

  return(rc);
}

int ldcs_audit_server_filemngt_store_file ( ldcs_message_t* msg, char **_filename, char **_dirname, char **_localpath, int *_domangle ) {
  int rc=0;
  FILE *outfile;
  char *p, *filename, *dirname, *fullname, *newname, *newdir;
  long  file_len;
  int filename_len, dirname_len, newname_len, fullname_len;
  char *mfilename;
  int domangle;
#ifdef LDCSDEBUG
  long  fwrote;
#endif
  
  p=msg->data;

  /* get domangle */
  memcpy(&domangle,p,sizeof(domangle)); p+=sizeof(domangle);

  /* get filename from message */
  memcpy(&filename_len,p,sizeof(filename_len)); p+=sizeof(filename_len);
  filename= (char*) malloc(filename_len); if(!filename) _error("could malloc memory for filename");
  memcpy(filename,p,filename_len);p+=filename_len;

  /* get dirname from message */
  memcpy(&dirname_len,p,sizeof(dirname_len)); p+=sizeof(dirname_len);
  dirname= (char*) malloc(dirname_len); if(!dirname) _error("could malloc memory for dirname");
  memcpy(dirname,p,dirname_len);p+=dirname_len;

  /* get fullname from message */
  memcpy(&fullname_len,p,sizeof(fullname_len)); p+=sizeof(fullname_len);
  fullname= (char*) malloc(fullname_len); if(!fullname) _error("could malloc memory for fullname");
  memcpy(fullname,p,fullname_len);p+=fullname_len;

  /* get file_len from message */
  memcpy(&file_len,p,sizeof(file_len)); p+=sizeof(file_len);

  
  newdir=(char*) malloc(strlen(_ldcs_audit_server_tmpdir)+2); if(!newdir) _error("could malloc memory for newdir");
  sprintf(newdir, "%s/",_ldcs_audit_server_tmpdir);

  if(domangle) {
    mfilename=strdup(fullname);
    mangleName(&mfilename);
    newname_len=strlen(_ldcs_audit_server_tmpdir)+1+strlen(mfilename)+1;
    newname= (char*) malloc(newname_len); if(!newname) _error("could malloc memory for newname");
    sprintf(newname, "%s/%s",_ldcs_audit_server_tmpdir,mfilename);
    free(mfilename);
  } else {
    newname_len=strlen(_ldcs_audit_server_tmpdir)+1+strlen(filename)+1;
    newname= (char*) malloc(newname_len); if(!newname) _error("could malloc memory for newname");
    sprintf(newname, "%s/%s",_ldcs_audit_server_tmpdir,filename);
  }
  debug_printf3("store file data for file: '%s'/'%s' (%s) %ld bytes  %10.4f MB to %s\n", 
	       dirname, filename, fullname, file_len, file_len/1024.0/1024.0, newname); 
  debug_printf3("store file data for file: '%s' to %s\n", filename, newname); 

  
  outfile = fopen(newname, "wb");
  if (!outfile)  _error("Could not open file");
  debug_printf3("  file open successful \n");
  
#ifdef LDCSDEBUG
  fwrote=
#endif
    _ldcs_file_write(outfile, p, file_len);
  fclose(outfile);
  debug_printf3("  file wrote and closed %ld bytes\n", fwrote);

  *_filename  = filename;
  *_dirname   = dirname;
  *_localpath = newname;
  *_domangle  = domangle;

  free(fullname);
  free(newdir);

  rc=file_len;
  
  return(rc);
}

long _ldcs_file_read(FILE *infile, void *data, int bytes ) {

  long       left,bsumread;
  long       btoread, bread;
  char      *dataptr;
  
  left      = bytes;
  bsumread  = 0;
  dataptr   = (char*) data;
  bread     = 0;

  while (left > 0)  {
    btoread    = left;
    debug_printf3("before read from file \n");
    bread      = fread(dataptr, 1, btoread, infile);
    if(bread<0) {
      debug_printf3("read from fifo: %ld bytes ... errno=%d (%s)\n",bread,errno,strerror(errno));
    } else {
      debug_printf3("read from fifo: %ld bytes ...\n",bread);
    }
    if(bread>0) {
      left      -= bread;
      dataptr   += bread;
      bsumread  += bread;
    } else {
      if(bread==0) return(bsumread);
      else         return(bread);
    }
  }
  return (bsumread);
}


long _ldcs_file_write(FILE *outfile, const void *data, int bytes ) {
  long         left,bsumwrote;
  long         bwrite, bwrote;
  char        *dataptr;
  
  left      = bytes;
  bsumwrote = 0;
  dataptr   = (char*) data;

  while (left > 0) {
    bwrite     = left;
    bwrote     = fwrite(dataptr, 1, bwrite, outfile);
    left      -= bwrote;
    dataptr   += bwrote;
    bsumwrote += bwrote;
  }
  return (bsumwrote);
}

/**
 * Clear files from the local ramdisk
 **/
int ldcs_audit_server_filemngt_clean()
{
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

   rmdir(_ldcs_audit_server_tmpdir);
   return 0;
}
