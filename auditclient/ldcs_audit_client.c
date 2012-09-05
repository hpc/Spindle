#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define _GNU_SOURCE
#define __USE_GNU
#include <link.h>

#include "ldcs_api.h" 
#include "config.h"

#define INTERCEPT_OPEN 1

#if defined(INTERCEPT_OPEN)
int intercept_open = 1;
#else
int intercept_open = 0;
#endif

static int (*orig_open)(const char *pathname, int flags, ...);
static int (*orig_open64)(const char *pathname, int flags, ...);
static FILE* (*orig_fopen)(const char *pathname, const char *mode);
static FILE* (*orig_fopen64)(const char *pathname, const char *mode);

#define VERYVERBOSEno 1

#define NOT_FOUND_PREFIX "/__not_exists/"
#define NOT_FOUND_PREFIX_SIZE 13

static int ldcsid=-1;
static int rankinfo[4]={-1,-1,-1,-1};

/* compare the pointer top the cookie not the cookie itself, it may be changed during runtime by audit library  */
static uintptr_t *firstcookie=NULL;
static signed long cookie_shift = 0;
/* static int use_ldcs = 1;  WF DEBUG*/
static int use_ldcs = 1;


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


/* rtld-audit interface functions */

unsigned int la_version(unsigned int version)
{
  char buf[256];
  int len;
  debug_printf("la_version(): %d\n", version);
  
  len=readlink("/proc/self/exe", buf, sizeof(buf)-1);
  if(len>0) {
    buf[len]='\0';
    debug_printf("la_version(): progname=%s\n", buf);
    /* switch of ldcs if exe is the starter */
    if ( 
	(strstr(buf,"/usr/bin/srun")!=NULL)
      ||
	(strstr(buf,"orterun")!=NULL)
      ||
	(strstr(buf,"/usr/bin/strace")!=NULL)
      ||
	(strstr(buf,"spindle_fe")!=NULL)
      ||
	(strstr(buf,"launchmon")!=NULL)
	 )
      {
      debug_printf("la_version(): prg is the starter disable caching ... \n");
      use_ldcs=0;
    }
  }

  /* printf("la_version(): %d\n", version);  */
   /* use_ldcs=0;	 */	/* WF */

  /* if(getpid()%128==0) { */
  /*   fprintf(stderr, "AUDITCLIENT for %s (%d) start time -> %14.4f\n",buf, getpid(), ldcs_get_time()); */
  /* } */

  return version;
}

char * la_objsearch(const char *name, uintptr_t *cookie, unsigned int flag)
{
  char *myname, *newname;

  myname=(char *) name;
  debug_printf("la_objsearch():         name = %s; cookie = %x; flag = %s\n", name, cookie,
	       (flag == LA_SER_ORIG) ?    "LA_SER_ORIG" :
	       (flag == LA_SER_LIBPATH) ? "LA_SER_LIBPATH" :
	       (flag == LA_SER_RUNPATH) ? "LA_SER_RUNPATH" :
	       (flag == LA_SER_DEFAULT) ? "LA_SER_DEFAULT" :
	       (flag == LA_SER_CONFIG) ?  "LA_SER_CONFIG" :
	       (flag == LA_SER_SECURE) ?  "LA_SER_SECURE" :
	       "???");

  if ((ldcsid==-1) && (use_ldcs)) {
    char* ldcs_location=getenv("LDCS_LOCATION");
    int   ldcs_number  =atoi(getenv("LDCS_NUMBER"));
    char* ldcs_locmodstr=getenv("LDCS_LOCATION_MOD");
    if(ldcs_locmodstr) {
      int ldcs_locmod=atoi(ldcs_locmodstr);
      char buffer[MAX_PATH_LEN];
      debug_printf("multiple server per node add modifier to location mod=%d\n",ldcs_locmod);
      if(strlen(ldcs_location)+10<MAX_PATH_LEN) {
	sprintf(buffer,"%s-%02d",ldcs_location,getpid()%ldcs_locmod);
	debug_printf("change location to %s (locmod=%d)\n",buffer,ldcs_locmod);
	debug_printf("open connection to ldcs %s %d\n",buffer,ldcs_number);
	ldcsid=ldcs_open_connection(buffer,ldcs_number);
      } else _error("location path too long");
    } else {
      debug_printf("open connection to ldcs %s %d\n",ldcs_location,ldcs_number);
      ldcsid=ldcs_open_connection(ldcs_location,ldcs_number);
    }
    if(ldcsid>-1) {
        ldcs_send_CWD(ldcsid);
        ldcs_send_HOSTNAME(ldcsid);
        ldcs_send_PID(ldcsid);
        ldcs_send_LOCATION(ldcsid,ldcs_location);
	ldcs_send_MYRANKINFO_QUERY (ldcsid, &rankinfo[0], &rankinfo[1], &rankinfo[2], &rankinfo[3]);
	fprintf(stderr, "AUDITCLIENT: pid=%6d local rank: %2d of %2d, MD rank: %2d of %2d start time -> %14.4f\n",
		getpid(), rankinfo[0],rankinfo[1],rankinfo[2],rankinfo[3],ldcs_get_time());
#if defined(SIONDEBUG)
	char* ldcs_auditdebug=getenv("LDCS_AUDITDEBUG");
	if(ldcs_auditdebug) {
	  if((rankinfo[0]==0) && ((rankinfo[2]%2)==0)) {
	    char filename[MAX_PATH_LEN];
	    sprintf(filename,"%s_%02d_%02d.log",ldcs_auditdebug,rankinfo[0],rankinfo[2]);
	    sion_debug_on(1023,filename);
	  }
	}
#endif
    }


  }

  /* check if direct name given --> return name  */
  {
    char *pos_slash = strchr(name, '/');
    if(!pos_slash) {
      debug_printf("Returning direct name %s after input %s\n", name, name);
      return (char *) name;
    }
  }

  
  debug_printf("AUDITSEND: L:%s\n",myname);
  /* fprintf(stderr,"AUDITSEND: L:%s\n",myname); */
  if ((ldcsid>=0) && (use_ldcs)) {
    ldcs_send_FILE_QUERY_EXACT_PATH(ldcsid,myname,&newname);
    debug_printf("AUDITRECV: L:%s\n",newname);
    /* fprintf(stderr,"AUDITRECV: %s:%s\n",myname,newname); */

    if(!newname) {
      newname=concatStrings(NOT_FOUND_PREFIX, NOT_FOUND_PREFIX_SIZE, myname, strlen(myname));
    }
    debug_printf(" found %s -> %s\n",myname,newname);
  } else {
    newname=myname;
  }


  debug_printf("la_objsearch():         name = %s; cookie = %x -> '%s'\n", name, cookie,newname);
  /* fprintf(stderr,"la_objsearch():         name = %s; cookie = %x -> '%s'\n", name, cookie,newname); */

  return newname;
}

void la_activity (uintptr_t *cookie, unsigned int flag)
{
  debug_printf("la_activity():          cookie = %x; flag = %s\n", cookie,
	 (flag == LA_ACT_CONSISTENT) ? "LA_ACT_CONSISTENT" :
	 (flag == LA_ACT_ADD) ?        "LA_ACT_ADD" :
	 (flag == LA_ACT_DELETE) ?     "LA_ACT_DELETE" :
	 "???");
  return;
}

unsigned int
la_objopen(struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
   signed long shift;

   debug_printf("la_objopen():           loading \"%s\"; lmid = %s; cookie=%x\n",
		map->l_name, (lmid == LM_ID_BASE) ?  "LM_ID_BASE" :
		(lmid == LM_ID_NEWLM) ? "LM_ID_NEWLM" : 
		"???", cookie);

   if (!firstcookie) {
     debug_printf("cookie store %d \n",*cookie);
     firstcookie=cookie;
   }
   
   shift = ((unsigned char *) map) - ((unsigned char *) cookie);
   if (cookie_shift) {
     assert(cookie_shift == shift);
   } else {
     cookie_shift = shift;
   }
   return LA_FLG_BINDTO | LA_FLG_BINDFROM;
}


void
la_preinit(uintptr_t *cookie)
{
  debug_printf("la_preinit():           %x\n", cookie);
}

unsigned int
la_objclose (uintptr_t *cookie)
{
  debug_printf("la_objclose():          %x\n", cookie);

  debug_printf("cookie compare %x %x\n",cookie,firstcookie);
  if(cookie == firstcookie) {
    if ((ldcsid>=0) && (use_ldcs)) {
#if defined(SIONDEBUG)
      /*
	char* ldcs_auditdebug=getenv("LDCS_AUDITDEBUG");
	if(ldcs_auditdebug) {
	  if((rankinfo[0]==0) && ((rankinfo[2]%2)==0)) {
	     ldcs_dump_memmaps(getpid()); 
	  }
	}
      */
#endif
      debug_printf("close connection %d\n",ldcsid);
      ldcs_send_END(ldcsid);
      ldcs_close_connection(ldcsid);
      
   
    }
  }

  return 0;
  
}

#if 0
uintptr_t
la_symbind32(Elf32_Sym *sym, unsigned int ndx, uintptr_t *refcook,
	     uintptr_t *defcook, unsigned int *flags, const char *symname)
{
#ifdef VERYVERBOSE
  debug_printf("la_symbind32():         symname = %s; sym->st_value = %p\n",
	 symname, sym->st_value);
  debug_printf("                        ndx = %d; flags = 0x%x", ndx, *flags);
  debug_printf("; refcook = %x; defcook = %x\n", refcook, defcook);
#endif	   

  return sym->st_value;
}

uintptr_t
la_symbind64(Elf64_Sym *sym, unsigned int ndx, uintptr_t *refcook,
	     uintptr_t *defcook, unsigned int *flags, const char *symname)
{
  printf("la_symbind64(): symname = %s; sym->st_value = %p\n",
	 symname, sym->st_value);
#ifdef VERYVERBOSE
  debug_printf("la_symbind64(): symname = %s; sym->st_value = %p\n",
	 symname, sym->st_value);
  debug_printf("        ndx = %d; flags = 0x%x", ndx, *flags);
  debug_printf("; refcook = %x; defcook = %x\n", refcook, defcook);
#endif	   

  return sym->st_value;
}
#endif

#ifdef BIT32
Elf32_Addr
la_i86_gnu_pltenter(Elf32_Sym *sym, unsigned int ndx,
		    uintptr_t *refcook, uintptr_t *defcook, La_i86_regs *regs,
		    unsigned int *flags, const char *symname, long int *framesizep)
{
#ifdef VERYVERBOSE
  debug_printf("la_i86_gnu_pltenter():  %s (%p)\n", symname, sym->st_value);
#endif	   

  return sym->st_value;
}
#endif

/**
 * open_filter returns 1 if we should redirect an open call
 * to a cached location, or 0 if we shouldn't.
 *
 * Currently setup to redirect reads of .py and .pyc files.
 **/
static int open_filter(const char *fname)
{
   const char *last_dot;
   last_dot = strrchr(fname, '.');
   if (!last_dot)
      return 0;
   if (strcmp(last_dot, ".py") == 0 || strcmp(last_dot, ".pyc") == 0 || strcmp(last_dot, ".so") == 0)
      return 1;

   return 0;
}

static int open_filter_flags(int flags)
{
   return !(((flags & O_WRONLY) == O_WRONLY) || ((flags & O_RDWR) == O_RDWR));
}

static int open_filter_str(const char *mode)
{
   if (!mode)
      return 1;
   while (*mode) {
      if (*mode == 'w' || *mode == 'a')
         return 0;
      mode++;
   }
   return 1;
}

/* returns:
    0 if not existent
   -1 could not check, use orig open
    1 exists, newpath contains real location */
int do_check_file(const char *path, char **newpath) {
  char *myname, *newname;
  
  myname=(char *) path;

  if ((ldcsid>=0) && (use_ldcs)) {
    debug_printf("AUDITSEND: E:%s\n",path);
    /* fprintf(stderr,"AUDITSEND: E:%s\n",path); */
    ldcs_send_FILE_QUERY_EXACT_PATH(ldcsid,myname,&newname);
    debug_printf("AUDITRECV: E:%s\n",newname);
    /* fprintf(stderr,"AUDITRECV: E %s:%s\n",myname,newname); */
  } else {
    debug_printf("no ldcs: open file query %s\n",myname);
    return -1;
  }

  if (newname != NULL) {
    *newpath=newname;
    debug_printf("file found under path %s\n",*newpath);
    return 1;
  } else {
    *newpath=NULL;
    errno = ENOENT;
    debug_printf("file not found file, set errno to ENOENT\n");
    return(0);
  }
}

static int rtcache_open(const char *path, int oflag, ...)
{
   va_list argp;
   mode_t mode;
   int rc=-1;
   char *newpath;
   
   va_start(argp, oflag);
   mode = va_arg(argp, mode_t);
   va_end(argp);

   debug_printf("open redirection of %s\n", path);

   if (!open_filter(path) || !open_filter_flags(oflag) || (ldcsid<0) )  {
     return orig_open(path, oflag, mode);
   } else {
     rc=do_check_file(path, &newpath);
   }
   if(rc==0)  return -1;
   else if(rc<0) {
     return orig_open(path, oflag, mode);
   } else {
     debug_printf(" redirect %s to %s\n", path, newpath);
     return orig_open(newpath, oflag, mode);
     free(newpath);
   }
   return -1;
}

static int rtcache_open64(const char *path, int oflag, ...)
{
   va_list argp;
   mode_t mode;
   int rc=-1;
   char *newpath;

   va_start(argp, oflag);
   mode = va_arg(argp, mode_t);
   va_end(argp);

   debug_printf("open64 redirection of %s\n", path);

   if (!open_filter(path) || !open_filter_flags(oflag) || (ldcsid<0) )  {
     return orig_open64(path, oflag, mode);
   } else {
     rc=do_check_file(path, &newpath);
   }
   if(rc==0)  return -1;
   else if(rc<0) {
     return orig_open64(path, oflag, mode);
   } else {
     debug_printf(" redirect %s to %s\n", path, newpath);
     return orig_open64(newpath, oflag, mode);
     free(newpath);
   }

   return -1;
}

static FILE *rtcache_fopen(const char *path, const char *mode)
{
   int rc;
   char *newpath;
   debug_printf("fopen redirection of %s\n", path);
   if (!open_filter(path) || !open_filter_str(mode) || (ldcsid<0) )  {
     return orig_fopen(path, mode);
   } else {
     rc=do_check_file(path, &newpath);
   }
   if(rc==0)  return NULL;
   else if(rc<0) {
     return orig_fopen(path, mode);
   } else {
     debug_printf(" redirect %s to %s\n", path, newpath);
     return orig_fopen(newpath, mode);
     free(newpath);
   }
   return NULL;
}

static FILE *rtcache_fopen64(const char *path, const char *mode)
{
   int rc;
   char *newpath=NULL;
   FILE *fp;

   debug_printf("fopen64 redirection of %s\n", path);

   if (!open_filter(path) || !open_filter_str(mode) || (ldcsid<0) )  {
     return orig_fopen64(path, mode);
   } else {
     rc=do_check_file(path, &newpath);
   }
   if(rc==0)  return NULL;
   else if(rc<0) {
     return orig_fopen64(path, mode);
   } else {
     debug_printf(" redirect %s to %s\n", path, newpath);
     fp=orig_fopen64(newpath, mode);
     debug_printf(" orig_fopen64 of %s %x\n", newpath, fp);
     free(newpath);
     return(fp); 
   }
   return NULL;
}

#ifndef arch_ppc64

static Elf64_Addr doPermanentBinding(struct link_map *map,
                              unsigned long plt_reloc_idx,
                              Elf64_Addr target)
{
   Elf64_Dyn *dynamic_section = map->l_ld;
   Elf64_Rela *rel = NULL;
   Elf64_Addr *got_entry;
   Elf64_Addr base = map->l_addr;
   for (; dynamic_section->d_tag != DT_NULL; dynamic_section++) {
      if (dynamic_section->d_tag == DT_JMPREL) {
         rel = ((Elf64_Rela *) dynamic_section->d_un.d_ptr) + plt_reloc_idx;
         break;
      }
   }
   if (!rel)
      return target;
   got_entry = (Elf64_Addr *) (rel->r_offset + base);
   *got_entry = target;
   return target;
}

Elf64_Addr la_x86_64_gnu_pltenter (Elf64_Sym *sym,
                                   unsigned int ndx,
                                   uintptr_t *refcook,
                                   uintptr_t *defcook,
                                   La_x86_64_regs *regs,
                                   unsigned int *flags,
                                   const char *symname,
                                   long int *framesizep)
{
   struct link_map *map = (struct link_map *) (((unsigned char *) refcook) + cookie_shift);
   unsigned long reloc_index = *((unsigned long *) (regs->lr_rsp-8));
   Elf64_Addr target;

   if (intercept_open && strstr(symname, "open")) {
      if (strcmp(symname, "open") == 0) {
         if (!orig_open)
            orig_open = (void *) sym->st_value;
         target = (uintptr_t) rtcache_open;
      }
      else if (strcmp(symname, "open64") == 0) {
         if (!orig_open64)
            orig_open64 = (void *) sym->st_value;
         target = (uintptr_t) rtcache_open64;
      }
      else if (strcmp(symname, "fopen") == 0) {
         if (!orig_fopen)
            orig_fopen = (void *) sym->st_value;
         target = (uintptr_t) rtcache_fopen;
      }
      else if (strcmp(symname, "fopen64") == 0) {
         if (!orig_fopen64)
            orig_fopen64 = (void *) sym->st_value;
         target = (uintptr_t) rtcache_fopen64;
      }
      else
         target = sym->st_value;
   }
   else
     target = sym->st_value;
   return doPermanentBinding(map, reloc_index, target);
}

#elif defined(arch_ppc64)

Elf64_Addr la_ppc64_gnu_pltenter (Elf64_Sym *sym,
				  unsigned int ndx,
				  uintptr_t *refcook,
				  uintptr_t *defcook,
				  La_ppc64_regs *regs,
				  unsigned int *flags,
				  const char *symname,
				  long int *framesizep)
{
   struct link_map *map = (struct link_map *) (((unsigned char *) refcook) + cookie_shift);
   /* unsigned long reloc_index = *((unsigned long *) (regs->lr_reg[2])); */
   Elf64_Addr target;

   /* printf("la_ppc64_gnu_pltenter():  %s (%p)\n", symname, sym->st_value); */
   /* printf("                          %s = %p %ul\n", "lr_r1", regs->lr_r1); */
   /* printf("                          %s = %p\n", "lr_lr", regs->lr_lr); */
   /* printf("                          %s = %ul\n", "reloc_index", reloc_index); */

   if (intercept_open && strstr(symname, "open")) {
      if (strcmp(symname, "open") == 0) {
         if (!orig_open)
            orig_open = (void *) sym->st_value;
         target = (uintptr_t) rtcache_open;
      }
      else if (strcmp(symname, "open64") == 0) {
         if (!orig_open64)
            orig_open64 = (void *) sym->st_value;
         target = (uintptr_t) rtcache_open64;
      }
      else if (strcmp(symname, "fopen") == 0) {
         if (!orig_fopen)
            orig_fopen = (void *) sym->st_value;
         target = (uintptr_t) rtcache_fopen;
      }
      else if (strcmp(symname, "fopen64") == 0) {
         if (!orig_fopen64)
            orig_fopen64 = (void *) sym->st_value;
         target = (uintptr_t) rtcache_fopen64;
      }
      else
         target = sym->st_value;
   }
   else
     target = sym->st_value;
   /* return doPermanentBinding(map, reloc_index, target);  */
   return target;
}
#endif

