/****************************************************************************
**  SIONLIB     http://www.fz-juelich.de/jsc/sionlib                       **
*****************************************************************************
**  Copyright (c) 2008-2011                                                **
**  Forschungszentrum Juelich, Juelich Supercomputing Centre               **
**                                                                         **
**  See the file COPYRIGHT in the package base directory for details       **
****************************************************************************/

/*!
 * @file printts.h
 * @brief Sion Time Stamp Header
 * @author Wolfgang Frings
 */

int sion_print_time_stamp (int rank, char *desc);
double _sion_get_time();
char  *_sion_get_time_asc();
