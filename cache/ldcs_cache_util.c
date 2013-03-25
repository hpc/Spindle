#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "ldcs_api.h"
#include "ldcs_cache.h"

int mangleFileDirName(const char *filename, const char *dirname, char **mname) {
  int rc=0, dirnamelen;
  char *newdirname; 
  dirnamelen=strlen(dirname);
  if(dirname[dirnamelen-1]!='/') {
    newdirname = concatStrings(dirname,dirnamelen , "/", 1);
    *mname = concatStrings(newdirname, strlen(newdirname), filename, strlen(filename));
    free(newdirname);
  } else {
    *mname = concatStrings(dirname, dirnamelen, filename, strlen(filename));
  }
  mangleName(mname);
  return(rc);
};

int mangleName(char **mname) {
  char *c;
  int rc=0;
  for(c=*mname;*c!='\0';c++) {
    if(*c=='/') *c='_';
  }
  return(rc);
};

int parseFilename(const char *name, const char *remote_cwd, char **filename, char **dirname) {
  int rc=0;
  char *lfn, *ldir, *lndir;
  breakUpFilename(name, &lfn, &ldir);
  debug_printf3("after breakUpFilename name='%s' filename=%s dirname=%s\n", name,lfn, ldir);
  if (!ldir) {
    *dirname  = ldir;
    *filename = lfn;
  } else {
    addCWDtoPath(ldir,remote_cwd,&lndir);
    debug_printf3(" after addCWDtoPath dirname='%s' remote_cwd=%s newdirname=%s\n", ldir,remote_cwd, lndir);
    *dirname  = lndir;
    *filename = lfn;
    free(ldir);
  }
  return(rc);
}

/* prepend also cwd if name has no slash, needed for exact path queries */
int parseFilenameExact(const char *name, const char *remote_cwd, char **filename, char **dirname) {
  int rc=0;
  char *lfn, *ldir, *lndir;
  breakUpFilename(name, &lfn, &ldir);
  debug_printf3("after breakUpFilename name='%s' filename=%s dirname=%s\n", name,lfn, ldir);
  if (!ldir) {
    addCWDtoPath("",remote_cwd,&lndir);
    *dirname  = lndir;
    *filename = lfn;
  }  else {
    addCWDtoPath(ldir,remote_cwd,&lndir);
    debug_printf3(" after addCWDtoPath dirname='%s' remote_cwd=%s newdirname=%s\n", *dirname,remote_cwd, lndir);
    *dirname  = lndir;
    *filename = lfn;
    free(ldir);
  }
  return(rc);
}

void addCWDtoPath(const char *path, const char *cwd, char **newpath) {
  char *newcwd=NULL; 
  *newpath=NULL;

  if(*path=='/') {
    *newpath = strdup(path);
    return;
  }
  
  newcwd = concatStrings(cwd, strlen(cwd), "/", 1);

  if(strlen(path)>=2) {
    if( (path[0]=='.') && (path[1]=='/') ) {
      *newpath = concatStrings(newcwd, strlen(newcwd), path+2, strlen(path)-2);
    }    
  }
  if(!*newpath){
    *newpath = concatStrings(newcwd, strlen(newcwd), path, strlen(path));
  }

  free(newcwd);
  return;
}

void breakUpFilename(const char *name, char **filename, char **dirname) {
  char *last_slash = strrchr(name, '/');

   if (last_slash) {
     debug_printf3(" before breakUpFilename name='%s'\n", name);
     *dirname = concatStrings(name, last_slash-name+1, NULL, 0);
     *filename = strdup(last_slash+1);
     debug_printf3(" after breakUpFilename name='%s' dirname='%s' filename='%s'\n", name, *dirname,*filename);

   }
   else {
     *dirname = NULL;
     *filename = strdup(name);
   }
}

char *concatStrings(const char *str1, int str1_len, const char *str2, int str2_len) {
   char *buffer = NULL;
   unsigned cur_size = str1_len + str2_len + 1;

   buffer = (char *) malloc(cur_size);
   strncpy(buffer, str1, str1_len);
   if (str2)
      strncpy(buffer+str1_len, str2, str2_len);
   buffer[str1_len+str2_len] = '\0';

   return buffer;
}

/* adapted from http://stackoverflow.com/questions/779875/what-is-the-function-to-replace-string-in-c */
char * replacePatString(
    char const * const original, 
    char const * const pattern, 
    char const * const replacement
) {
  size_t const replen = strlen(replacement);
  size_t const patlen = strlen(pattern);
  size_t const orilen = strlen(original);

  size_t patcnt = 0;
  const char * oriptr;
  const char * patloc;

  // find how many times the pattern occurs in the original string
  for (oriptr = original; (patloc = strstr(oriptr, pattern)); oriptr = patloc + patlen)
  {
    patcnt++;
  }

  {
    // allocate memory for the new string
    size_t const retlen = orilen + patcnt * (replen - patlen);
    char * const returned = (char *) malloc( sizeof(char) * (retlen + 1) );

    if (returned != NULL)
    {
      // copy the original string, 
      // replacing all the instances of the pattern
      char * retptr = returned;
      for (oriptr = original; (patloc = strstr(oriptr, pattern)); oriptr = patloc + patlen)
      {
        size_t const skplen = patloc - oriptr;
        // copy the section until the occurence of the pattern
        strncpy(retptr, oriptr, skplen);
        retptr += skplen;
        // copy the replacement 
        strncpy(retptr, replacement, replen);
        retptr += replen;
      }
      // copy the rest of the string.
      strcpy(retptr, oriptr);
    }
    return returned;
  }
}
