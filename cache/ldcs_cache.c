#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>

#include <stddef.h>

#include "ldcs_api.h"
#include "ldcs_cache.h"
#include "ldcs_hash.h"

ldcs_cache_result_t ldcs_cache_findDirInCache(char *dirname) {
   struct ldcs_hash_entry_t *e = ldcs_hash_Lookup(dirname);
   if(e) {
     debug_printf("directory entry exists %d %x %x '%s' '%s'\n",e->dirname == e-> filename,e->dirname,e-> filename, e->dirname,e-> filename);
     return (strcmp(e->dirname,e-> filename)==0) ? LDCS_CACHE_DIR_PARSED_AND_EXISTS : LDCS_CACHE_DIR_PARSED_AND_NOT_EXISTS;
   } else {
     return LDCS_CACHE_DIR_NOT_PARSED;
   }
}

ldcs_cache_result_t ldcs_cache_findFileInCache(char *filename, char **newname, char **localpath) {
   struct ldcs_hash_entry_t *e = ldcs_hash_Lookup(filename);
   if(e) {
     *newname= concatStrings(e->dirname, strlen(e->dirname), filename, strlen(filename));
     if(e->ostate==LDCS_CACHE_OBJECT_STATUS_LOCAL_PATH) {
       *localpath=strdup(e->localpath);
     } else {
       *localpath=NULL;
     }
     return(LDCS_CACHE_FILE_FOUND);
   } else {
     *newname=NULL;
     return(LDCS_CACHE_FILE_NOT_FOUND);
   }
}

ldcs_cache_result_t ldcs_cache_findFileInCachePrio(char *filename, char **newname, char **localpath) {
  struct ldcs_hash_entry_t *e = ldcs_hash_Lookup_FN_and_Ostate(filename,LDCS_CACHE_OBJECT_STATUS_LOCAL_PATH);
  if(!e) {
    e = ldcs_hash_Lookup(filename);
  }
  if(e) {
    *newname= concatStrings(e->dirname, strlen(e->dirname), filename, strlen(filename));
    if(e->ostate==LDCS_CACHE_OBJECT_STATUS_LOCAL_PATH) {
      *localpath=strdup(e->localpath);
    } else {
      *localpath=NULL;
    }
    return(LDCS_CACHE_FILE_FOUND);
  } else {
    *newname=NULL;
    return(LDCS_CACHE_FILE_NOT_FOUND);
  }
}


ldcs_cache_result_t ldcs_cache_findFileDirInCache(char *filename, char *dirname, char **newname, char **localpath) {
  struct ldcs_hash_entry_t *e = ldcs_hash_Lookup_FN_and_DIR(filename,dirname);
   if(e) {
     *newname= concatStrings(e->dirname, strlen(e->dirname), filename, strlen(filename));
     if(e->ostate==LDCS_CACHE_OBJECT_STATUS_LOCAL_PATH) {
       *localpath=strdup(e->localpath);
     } else {
       *localpath=NULL;
     }
     return(LDCS_CACHE_FILE_FOUND);
   } else {
     *newname=NULL;
     return(LDCS_CACHE_FILE_NOT_FOUND);
   }
}

ldcs_cache_result_t ldcs_cache_findFileDirInCachePrio(char *filename, char *dirname, char **newname, char **localpath) {
  struct ldcs_hash_entry_t *e = ldcs_hash_Lookup_FN_and_DIR_Ostate(filename,dirname,LDCS_CACHE_OBJECT_STATUS_LOCAL_PATH);
  if(!e) {
    e = ldcs_hash_Lookup_FN_and_DIR(filename,dirname);
  }
  if(e) {
    *newname= concatStrings(e->dirname, strlen(e->dirname), filename, strlen(filename));
    if(e->ostate==LDCS_CACHE_OBJECT_STATUS_LOCAL_PATH) {
      *localpath=strdup(e->localpath);
    } else {
      *localpath=NULL;
    }
    return(LDCS_CACHE_FILE_FOUND);
  } else {
    *newname=NULL;
    return(LDCS_CACHE_FILE_NOT_FOUND);
  }
}

ldcs_cache_result_t ldcs_cache_processDirectory(char *dirname) {
  debug_printf("Processing directory %s\n", dirname);
  if (directoryParsed(dirname)) {
    debug_printf("Directory %s already parsed\n", dirname);
    return(LDCS_CACHE_DIR_PARSED_AND_EXISTS);
  }
  cacheLibraries(dirname);
  
  return(ldcs_cache_findDirInCache(dirname));
}

ldcs_cache_result_t ldcs_cache_updateLocalPath(char *filename, char *dirname, char *localpath) {
  struct ldcs_hash_entry_t *e = ldcs_hash_updateEntryLocalPath(dirname, filename, localpath);
   if(e) {     return(LDCS_CACHE_FILE_FOUND);   } 
   else  {    return(LDCS_CACHE_FILE_NOT_FOUND); }
}

ldcs_cache_result_t ldcs_cache_updateStatus(char *filename, char *dirname, ldcs_hash_object_status_t ostate) {
  struct ldcs_hash_entry_t *e = ldcs_hash_updateEntryOState(filename, dirname, (int) ostate);
  if(e) {     return(LDCS_CACHE_FILE_FOUND);   } 
  else  {    return(LDCS_CACHE_FILE_NOT_FOUND); }
}

ldcs_hash_object_status_t ldcs_cache_getStatus(char *filename) {
  struct ldcs_hash_entry_t *e = ldcs_hash_Lookup(filename);
  if(e) {     return((ldcs_hash_object_status_t) e->ostate);   } 
  else  {    return(LDCS_CACHE_OBJECT_STATUS_UNKNOWN); }
}

ldcs_hash_object_status_t ldcs_cache_addEntry(char *filename, char *dirname) {
  ldcs_hash_addEntry(dirname, filename);

  struct ldcs_hash_entry_t *e = ldcs_hash_Lookup(filename);
  if(e) {     return((ldcs_hash_object_status_t) e->ostate);   } 
  else  {    return(LDCS_CACHE_OBJECT_STATUS_UNKNOWN); }
}


int ldcs_cache_getNewEntriesSerList(char **data, int *len) {
  int rc=0;
  char *ser_data=NULL, *p;
  int ser_len=0, index, length_fn, length_dir, c, sum;
  struct ldcs_hash_entry_t *entry;
  struct ldcs_hash_entry_t * *new_entries=NULL;
  int new_entries_size=0;
  int new_entries_used=0;

  debug_printf(" CACHE: start getNewEntries \n");
  entry=ldcs_hash_getFirstNewEntry();
  while(entry!=NULL) {
    /* debug_printf(" CACHE: got entry '%s' '%s'\n",entry->filename,entry->dirname); */
    if(new_entries_used>=new_entries_size) {
      new_entries=realloc(new_entries,(new_entries_size+100)*sizeof(struct hash_entry_t *));
      new_entries_size+=100;
      debug_printf(" CACHE: realloc %d of %d\n",new_entries_used,new_entries_size);
    }
    new_entries[new_entries_used]=entry;
    new_entries_used++;

    length_fn = (entry->filename)?strlen(entry->filename):0;
    length_dir= (entry->dirname)?strlen(entry->dirname):0;

    ser_len+=2*sizeof(int)+length_fn+length_dir + 2; /* for \0 */
    /*    debug_printf("found entry #%03d: %d:%s %d:%s  ser_len=%d\n",new_entries_used,
		 length_fn, entry->filename,
		 length_dir, entry->dirname,		 ser_len); */
    entry=ldcs_hash_getNextNewEntry();
  }

  if(ser_len==0) {
    *len=0;*data=NULL;
    return(0);
  }
  
  ser_data=(char *) malloc(ser_len);
  if (!ser_data)  _error("could not allocate memory for serialized new entries");
  
  p=ser_data;
  for(index=0;index<new_entries_used;index++) {
    entry=new_entries[index];
    length_fn = (entry->filename)?strlen(entry->filename):0;
    length_dir= (entry->dirname)?strlen(entry->dirname):0;
    
    /*    debug_printf("Entry #%03d: %d:%s %d:%s offset %d\n",index,
		 length_fn, entry->filename,
		 length_dir, entry->dirname, p-ser_data ); */
    memcpy(p,&length_fn,sizeof(int));p+=sizeof(int);
    if(length_fn) {memcpy(p,entry->filename,length_fn+1);p+=length_fn+1;}
    memcpy(p,&length_dir,sizeof(int));p+=sizeof(int);
    if(length_dir) {memcpy(p,entry->dirname,length_dir+1);p+=length_dir+1;}
  }

  *data=ser_data;
  *len=ser_len;

  p=ser_data;sum=0;
  for(c=0;c<ser_len;c++) sum+=ser_data[c];
  debug_printf("send now %d bytes sum=%8d\n",ser_len, sum );
  
  free(new_entries);
  
  debug_printf(" CACHE: end getNewEntries \n");

  return(rc);
}

int   ldcs_cache_storeNewEntriesSerList(char *ser_data, int ser_len) {
  int rc=0;
  char *p, *filename, *dirname;
  int pos, length, count, c, sum;

  p=ser_data;sum=0;
  for(c=0;c<ser_len;c++) sum+=ser_data[c];
  debug_printf("got %d bytes sum=%8d\n",ser_len, sum );

  p=ser_data;pos=0;count=0;
  while(pos<ser_len) {
    /* debug_printf("Next entry #%03d: at offset %d\n",count, p-ser_data ); */

    memcpy(&length,p,sizeof(int));p+=sizeof(int);pos+=sizeof(int);
    filename=(length)?p:NULL;p+=length+1;pos+=length+1;
    /* debug_printf("Entry #%03d: fn=  %d:%s \n",count, length, filename ); */

    memcpy(&length,p,sizeof(int));p+=sizeof(int);pos+=sizeof(int);
    dirname=(length)?p:NULL;p+=length+1;pos+=length+1;
    /* debug_printf("Entry #%03d: dir= %d:%s \n",count, length, dirname ); */

    ldcs_hash_addEntry(dirname, filename);count++;
  }
  
  return(rc);
}

int ldcs_cache_init() {
  int rc=0;
  ldcs_hash_init();
  return(rc);
}

int ldcs_cache_dump(char *filename) {
  int rc=0;
  ldcs_hash_dump(filename);
  return(rc);
}




char *findDirForFile(const char *name, const char *remote_cwd) {
   char *dirname, *found_dirname, *result;
   char *filename;
   char *newdirname;

   breakUpFilename(name, &filename, &dirname);
   debug_printf(" after breakUpFilename name='%s' filename=%s dirname=%s\n", name,filename,dirname);

   if (!dirname) {
      debug_printf("Returning direct name %s after input '%s'\n", name, name);
      printf(" CACHE: returning direct name for: '%s' \n", name);
      return (char *) strdup(name);
   }
      
   addCWDtoPath(dirname,remote_cwd,&newdirname);
   debug_printf(" after addCWDtoPath dirname='%s'remote_cwd=%s newdirename=%s\n", dirname,remote_cwd, newdirname);

   processDirectory(newdirname);

   debug_printf("lookup for file '%s' name=%s newndirame=%s\n", dirname,name,newdirname);
   
  if (!directoryParsedAndExists(newdirname)) {
      debug_printf("directory %s does not exists. Returning NULL\n", newdirname);
      printf(" CACHE: directory does not exists: '%s' \n", newdirname);
      free(newdirname); free(dirname);
      return NULL;
  } else {
    free(newdirname); free(dirname);
  }

   found_dirname = lookupDirectoryForFile(filename);
   if (!found_dirname) {
      debug_printf("File %s not found in cache. Returning NULL\n", filename);
      printf(" CACHE: file not found in cache for: '%s' \n", filename);
      return NULL;
   }

   result = concatStrings(found_dirname, strlen(found_dirname), filename, strlen(filename));
   debug_printf("Returning %s after input %s\n", result, name);
   printf(" CACHE: file found in cache for: '%s' -> '%s' \n", filename,result);
   return result;
}

void processDirectory(char *dirname) {

  debug_printf("Processing directory %s\n", dirname);
  if (directoryParsed(dirname)) {
    debug_printf("Directory %s already parsed\n", dirname);
    return;
  }
  cacheLibraries(dirname);
}

char *lookupDirectoryForFile(const char *filename) {
   struct ldcs_hash_entry_t *e = ldcs_hash_Lookup(filename);
   return e ? e->dirname : NULL;
}

int directoryParsed(char *dirname) {
   struct ldcs_hash_entry_t *e = ldcs_hash_Lookup(dirname);
   return e ? 1 : 0;
}

int directoryParsedAndExists(char *dirname) {
   struct ldcs_hash_entry_t *e = ldcs_hash_Lookup(dirname);
   if(e) {
     return (strcmp(e->dirname,e-> filename)==0) ? 1 : 0;
   } else {
     return 0;
   }
}

void cacheLibraries(char *dirname) {
   size_t len;
   int count;
   debug_printf("cacheLibraries for directory %s\n", dirname);
   
   DIR *d = opendir(dirname);
   struct dirent *dent = NULL, *entry;

   if (!d) {
     ldcs_hash_addEntry("-", dirname);
     debug_printf("Could not open directory %s, empty entry added\n", dirname);
     return;
   } else {
     ldcs_hash_addEntry(dirname, dirname);
   }


   len = offsetof(struct dirent, d_name) + pathconf(dirname, _PC_NAME_MAX) + 1;
   entry = (struct dirent *) malloc(len);
   count = 0;

   for (;;) {
      if (readdir_r(d, entry, &dent) != 0)
         break;
      if (dent == NULL)
         break;
      if (dent->d_type != DT_LNK && dent->d_type != DT_REG && dent->d_type != DT_UNKNOWN) {
	//         debug_printf("Not adding file %s%s due to being non-so type\n", dirname, dent->d_name);
         continue;
      }
      
      ldcs_hash_addEntry(dirname, dent->d_name);count++;
   }

   /* printf(" CACHE: directory scan of: %s --> %d elements found\n", dirname, count); */
   closedir(d);

   free(entry);
}
