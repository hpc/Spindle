/****************************************************************************
**  SIONLIB     http://www.fz-juelich.de/jsc/sionlib                       **
*****************************************************************************
**  Copyright (c) 2008-2011                                                **
**  Forschungszentrum Juelich, Juelich Supercomputing Centre               **
**                                                                         **
**  See the file COPYRIGHT in the package base directory for details       **
****************************************************************************/

/*!
 * \file sion_debug.c
 * \brief Debugging output
 *
 * \author Th.Eickermann & W.Frings (January 2001)
 *
 * Print debugging info according to the following environment variables:
 * <ul>
 *   <li>   SION_DEBUG        = filename - print to "filename" or stdout if filename is empty
 *   <li>   SION_MASK         = Binary mask used to select which messages should be logged
 *   <li>   SION_DEBUG_RANK   = Specifies for which rank to print the messages
 *   <li>   SION_DEBUG_RANK1  = The same as SION_DEBUG_RANK
 *   <li>   SION_DEBUG_RANK2  = Specifies a second rank to log
 * </ul>
 *
 * The value for the mask can be calculated by adding the following values:
 * <ul>
 *   <li>   1 -> sion user function entries and exits
 *   <li>   2 -> sion internal function entries and exits
 *   <li>   8 -> high frequently called sion user function entries and exits (sion_feof, sion_ensure_free_space, ...)
 *   <li>  16 -> high frequently called sion user function entries and exits (internal steps)
 *   <li>  32 -> high frequently called sion internal function (internal steps)
 *   <li> 128 -> timings (top level)
 *   <li> 256 -> timings (low level)
 *   <li> 512 -> elg lib
 *   <li>1024 -> gz lib
 *   <li>2048 -> higher frequently called sion internal function (internal steps)
 * </ul>
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "sion_debug.h"

#define SIONDEBFUNCNAMELEN 30
#define MAXOMPTHREADS 20


int _sion_get_thread_num_default();
static int (*my_get_thread_num)() = _sion_get_thread_num_default;           

/*!< first call of dprintf? */
static int first[MAXOMPTHREADS] = {1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1};           

 /*!< debug output is printed to out */
static FILE *out[MAXOMPTHREADS] = { NULL,NULL,NULL,NULL,NULL,  NULL,NULL,NULL,NULL,NULL,  
				    NULL,NULL,NULL,NULL,NULL,  NULL,NULL,NULL,NULL,NULL};       

/*!< true, if debug output is desired */
static int isdebug[MAXOMPTHREADS] = {0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0}; 

 /*!< MPI rank of process */
static int myrank[MAXOMPTHREADS] = {-1,-1,-1,-1,-1, -1,-1,-1,-1,-1, -1,-1,-1,-1,-1, -1,-1,-1,-1,-1};        
static int debmask = 1023;      /*!< mask to define the quantity of debug output */
static int debrank1 = -1;       /*!< first rank for which to log debug information */
static int debrank2 = -1;       /*!< second rank for which to log debug information */

#define _DEBUG_INIT_THREAD \
   int threadid=0; \
   threadid = my_get_thread_num(); \
   if(threadid>MAXOMPTHREADS) threadid=(MAXOMPTHREADS-1); 


/* default thread number */
int _sion_get_thread_num_default() {
  return(0);
}

/* default thread number */
int _sion_debug_set_query_thread_num_function( int (*get_thread_num)() ) {
  my_get_thread_num=get_thread_num;
  return(1);
}

/*!
 * \brief Print debugging info formating the message like printf
 *
 */
int sion_dprintf(int mask, const char *format, ...)
{
  va_list   ap;
  _DEBUG_INIT_THREAD
  
  if (first[threadid])
    _sion_debug_init();

  if ((!isdebug[threadid]) || !(mask & debmask))
    return 1;

  fprintf(out[threadid], "    ");

  va_start(ap, format);

  vfprintf(out[threadid], format, ap);

  va_end(ap);

  return 1;
}

/*!
 * \brief Print debugging info formating the message like printf and including the name of the calling function
 *
 */
int sion_dprintfp(int mask, const char *callfunction, int rank, const char *format, ...)
{
  va_list   ap;
  static char tmpfuncname[SIONDEBFUNCNAMELEN + 1];
  static char spec[20];
  int       setrank = 0, norank = 0;
  _DEBUG_INIT_THREAD

  if ((myrank[threadid] < 0) && (rank < 0)) {
    return (0);
  }

  /* no rank specified, used previous stored rank */
  if (rank < 0) {
    rank = myrank[threadid];
    norank = 1;
  }

  /* if internal rank is not initialized used rank parameter, otherwise don't overwrite it  */
  if (myrank[threadid] < 0) {
    myrank[threadid] = rank;
    setrank = 1;
  }
  /*   if(setrank) */
  /*     fprintf(stderr,"WF: in sion_dprintfp: mask=%d %s rank=%d myrank=%d setrank=%d norank=%d first=%d\n",mask,callfunction,rank,myrank,setrank,norank,first);  */

  if (first[threadid])
    _sion_debug_init();

  /*   fprintf(stderr,"WF: in sion_dprintfp: mask=%d %s rank=%d\n",mask,callfunction,rank); */

  if ((!isdebug[threadid]) || !(mask & debmask))
    return 1;
  if ((debrank1 >= 0) && (debrank2 >= 0)) {
    if ((rank != debrank1) && (rank != debrank2))
      return 1;
  }
  else if (debrank1 >= 0) {
    if (rank != debrank1)
      return 1;
  }

  if (strlen(callfunction) > SIONDEBFUNCNAMELEN) {
    strncpy(tmpfuncname, callfunction, SIONDEBFUNCNAMELEN);
    tmpfuncname[SIONDEBFUNCNAMELEN] = '\0';
  }
  else
    strcpy(tmpfuncname, callfunction);

  sprintf(spec, "SION[%s%s%%5d][%%-%ds] ", (setrank ? "S" : " "), (norank ? "N" : " "), SIONDEBFUNCNAMELEN);
  fprintf(out[threadid], spec, rank, tmpfuncname);
  if (mask >= 8)
    fprintf(out[threadid], "    ");
  if (mask > 64)
    fprintf(out[threadid], "    ");
  if (mask >= 128)
    fprintf(out[threadid], "    ");

  va_start(ap, format);

  vfprintf(out[threadid], format, ap);

  va_end(ap);

  fflush(out[threadid]);

  return 1;
}

/*!
 * return the file pointer of the debug file
 */
FILE     *sion_get_dfile(void)
{
  _DEBUG_INIT_THREAD
  if (first[threadid])
    _sion_debug_init();

  return out[threadid];
}

/*!
 * close the debug-file
 */
void sion_dclose(void)
{
  _DEBUG_INIT_THREAD
  if (out[threadid] && (out[threadid] != stdout) && (out[threadid] != stderr)) {
    fclose(out[threadid]);
    out[threadid] = NULL;
  }
}

int sion_isdebug(void)
{
  _DEBUG_INIT_THREAD
  if (first[threadid])
    _sion_debug_init();

  return isdebug[threadid] ? debmask : 0;
}


/*!
 * \brief sets debug mode.
 *
 * \param mask debug mask
 * \param filename if not NULL, output file for debug output (default is stderr)
 */
void sion_debug_on(int mask, const char *filename)
{
  char     *fname = 0;
  _DEBUG_INIT_THREAD

  if (out[threadid])
    sion_dclose();              /* close previously opened logfile */

  first[threadid] = 0;                    /* do not call _sion_debug_init() */

  if (filename) {
    fname = (char *) malloc((strlen(filename) + 1) * sizeof(char));
    strcpy(fname, filename);
  }

  /* open debug output file (default is stderr) */
  if (!fname || (strlen(fname) == 0) || !strcmp(fname, "stderr")) {
    out[threadid] = stderr;
  }
  else if (!strcmp(fname, "stdout")) {
    out[threadid] = stdout;
  }
  else if (!(out[threadid] = fopen(fname, "w"))) {
    fprintf(stderr, "sion_dprintf: failed to open '%s' for writing\n", fname);
    out[threadid] = stderr;
  }

#ifdef SION_DEBUG_EXTREME
  fprintf(stderr, "Warning: you are using a version of SION that is configured with -DEBUG (current debug-mask is %d\n", debmask);
#endif

  if ((out[threadid] != stdout) && (out[threadid] != stderr))
    fprintf(stderr, "Writing debug output to %s\n", fname);

  if (fname)
    free(fname);

  debmask = mask;
  isdebug[threadid] = 1;
}

void sion_debug_off(void)
{
  _DEBUG_INIT_THREAD
  isdebug[threadid] = 0;
  sion_dclose();
}


/*!
 * \brief initialize the debug environment
 *
 * evaluate the debug environment variables
 * set isdebug and out
 */

int _sion_debug_init(void)
{
  int rvalue = 1;
  _DEBUG_INIT_THREAD
  {
    if (first[threadid]) {
      const char *t;
      char     *filename = 0;

      first[threadid] = 0;
      isdebug[threadid] = 0;


      t = getenv("SION_DEBUG_RANK");
      if (t)
	debrank1 = atoi(t);

      t = getenv("SION_DEBUG_RANK1");
      if (t)
	debrank1 = atoi(t);

      t = getenv("SION_DEBUG_RANK2");
      if (t)
	debrank2 = atoi(t);

      t = getenv("SION_DEBUG");
      if (t) {
	filename = (char *) malloc((strlen(t) + 1 + 10) * sizeof(char));
	sprintf(filename, "%s.%05d", t, myrank[threadid]);
      }

      t = getenv("SION_DEBUG_MASK");
      if (t)
	debmask = atoi(t);


      if (filename)
	isdebug[threadid] = 1;              /* set debug mode if SION_DEBUG ist set */

      if ((debrank1 >= 0) && (debrank2 >= 0)) {
	if ((myrank[threadid] != debrank1) && (myrank[threadid] != debrank2))
	  isdebug[threadid] = 0;
      }
      else if (debrank1 >= 0) {
	if (myrank[threadid] != debrank1)
	  isdebug[threadid] = 0;
      }

      if ((debrank1 == -2) && (debrank2 == -2)) {
	isdebug[threadid] = 1;
      }
      if (isdebug[threadid]) {
	if ((filename == 0) || (strlen(filename) == 0) || !strcmp(filename, "stderr")) {
	  out[threadid] = stderr;
	}
	else if (!strcmp(filename, "stdout")) {
	  out[threadid] = stdout;
	}
	else if (!(out[threadid] = fopen(filename, "w"))) {
	  fprintf(out[threadid], "sion_dprintf: failed to open '%s' for writing\n", filename);
	  rvalue = 0;
	}

	if( rvalue && ((out[threadid] != stdout) && (out[threadid] != stderr)))
	  fprintf(stderr, "Writing debug output to %s\n", filename);

      }

      if (filename) free(filename);

      /* print a warning (to avoid using accidentally
	 a library that is full of DPRINTFs) */
#ifdef SION_DEBUG_EXTREME
      fprintf(stderr, "Warning: you are using a version of SION that is configured with --with-debug (current debug-mask is %d)\n", debmask);
#endif

    }
  }
  return rvalue;
}

