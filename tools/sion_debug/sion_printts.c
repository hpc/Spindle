/****************************************************************************
**  SIONLIB     http://www.fz-juelich.de/jsc/sionlib                       **
*****************************************************************************
**  Copyright (c) 2008-2011                                                **
**  Forschungszentrum Juelich, Juelich Supercomputing Centre               **
**                                                                         **
**  See the file COPYRIGHT in the package base directory for details       **
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

#include "sion_printts.h"
#include "sion_debug.h"

/*!
 * @file sion_printts.c
 * @brief Sion Time Stamp
 * @author Wolfgang Frings
 */

#if defined(_SION_BGP) && defined(SION_MPI)

#include <spi/kernel_interface.h>
#include <common/bgp_personality.h>
#include <common/bgp_personality_inlines.h>

static double clockspeed = 1.0e-6 / 850.0;

double bgp_wtime()
{
  return (_bgp_GetTimeBase() * clockspeed);
  /*return(1.0); */
}
#elif defined(_SION_BGQ) && defined(SION_MPI)

#include <firmware/include/personality.h>
#include <spi/include/kernel/process.h>
#include <spi/include/kernel/location.h>
#ifdef __GNUC__
#include <hwi/include/bqc/A2_inlines.h>   // for GetTimebase()
#endif
#include <hwi/include/common/uci.h>

static double clockspeed = 1.0e-6 / 850.0;

double bgq_wtime()
{
#ifdef __IBMC__
  return ( __mftb() * elg_clockspeed );
#elif defined __GNUC__
  return ( GetTimeBase() * elg_clockspeed );
#else
#error "Platform BGQ: cannot determine timebase"
#endif
}
#elif defined(_BGL)
#include <rts.h>

static double clockspeed = 1.0e-6 / 700.0;

double bgl_wtime()
{
  return (rts_get_timebase() * clockspeed);
}
#endif

/*!
 * @brief Returns the current time as a double
 */
double _sion_get_time()
{
#if defined(_SION_BGL) && defined(SION_MPI)
  return bgl_wtime();
#elif defined(_SION_BGQ) && defined(SION_MPI)
  return bgp_wtime();
#elif defined(_SION_BGP) && defined(SION_MPI)
  return bgp_wtime();
#else
  struct timeval tp;
  gettimeofday (&tp, (struct timezone *)NULL);
  return tp.tv_sec + tp.tv_usec/1000000.0;
#endif
}

/*!
 * @brief Returns the current time as a string
 */
char     *_sion_get_time_asc()
{
  time_t    t;
  struct tm *ts;
  static char asctimestr[256];
#if defined(_SION_BGL)
  sprintf(asctimestr, "%18.8fs\n", _sion_get_time());
#elif defined(_SION_BGQ)
  sprintf(asctimestr, "%18.8fs\n", _sion_get_time());
#elif defined(_SION_BGP)
  sprintf(asctimestr, "%18.8fs\n", _sion_get_time());
#else
  t = time(NULL);
  ts = localtime(&t);
  sprintf(asctimestr, "%s", asctime(ts));
#endif
  return (asctimestr);
}

/*!
 * @brief Prints a sion time stamp
 *
 * @param[in]   rank    Current task number
 * @param[in]   desc    Description of the time stamp
 *
 * @return      1
 */
int sion_print_time_stamp(int rank, char *desc)
{
  time_t    t;
#if defined(_BGL)
#else
  struct tm *ts;
#endif

  /* return (1); */

  if (!sion_isdebug())
    return (1);

#if defined(_BGL)
  DPRINTFP((32, "TIMINGS", rank, "                            step=%-18s timestamp=%18.8f\n", desc, bgl_wtime()));
#else
  t = time(NULL);
  ts = localtime(&t);
  DPRINTFP((32, "TIMINGS", rank, "                            step=%-18s timestamp=%18.8f\n", desc, asctime(ts)));
#endif
  return (1);
}
