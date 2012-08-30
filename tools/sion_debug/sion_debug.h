/****************************************************************************
**  SIONLIB     http://www.fz-juelich.de/jsc/sionlib                       **
*****************************************************************************
**  Copyright (c) 2008-2011                                                **
**  Forschungszentrum Juelich, Juelich Supercomputing Centre               **
**                                                                         **
**  See the file COPYRIGHT in the package base directory for details       **
****************************************************************************/
#ifndef SION_SION_DEBUG_H
#define SION_SION_DEBUG_H

#ifdef DPRINTF
#undef DPRINTF
#undef DPRINTFP
#undef DPRINTTS
#undef DPRINTFTS2
#endif
#ifdef SION_DEBUG
#define DPRINTF(A) sion_dprintf A
#define DPRINTFP(A) sion_dprintfp A
#define DPRINTFTS(rank,desc)  sion_dprintfp(128,"TIMING",rank,"                                                       step=%-40s timestamp=%20s",desc,_sion_get_time_asc())
#define DPRINTFTS2(rank,desc) sion_dprintfp(256,"TIMING",rank,"                                                       step=%-40s timestamp=%20s",desc,_sion_get_time_asc())
#else
#define DPRINTF(A)
#define DPRINTFP(A)
#define DPRINTFTS(A,B)
#define DPRINTFTS2(A,B)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "sion_printts.h"

int _sion_debug_init(void);
int sion_dprintf(int mask, const char *format, ...);
int sion_dprintfp(int mask, const char *callfunction, int rank, const char *format, ...);
int sion_isdebug(void);
void sion_debug_on(int mask, const char *filename);
void sion_debug_off(void);
FILE *sion_get_dfile(void);
void sion_dclose(void);

void _sion_debug_set_lock_cb(void (*set_lock)(), void (*unset_lock)());

int _sion_debug_set_query_thread_num_function( int (*get_thread_num)() );

#ifdef __cplusplus
}
#endif

#endif
