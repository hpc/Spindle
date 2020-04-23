/*
  This file is part of Spindle.  For copyright information see the COPYRIGHT 
  file in the top level directory, or at 
  https://github.com/hpc/Spindle/blob/master/COPYRIGHT

  This program is free software; you can redistribute it and/or modify it under
  the terms of the GNU Lesser General Public License (as published by the Free Software
  Foundation) version 2.1 dated February 1999.  This program is distributed in the
  hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
  WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
  and conditions of the GNU Lesser General Public License for more details.  You should 
  have received a copy of the GNU Lesser General Public License along with this 
  program; if not, write to the Free Software Foundation, Inc., 59 Temple
  Place, Suite 330, Boston, MA 02111-1307 USA
*/

#if !defined(GCCWARNS_H_)
#define GCCWARNS_H_

#if defined(__GNUC__)

#define DOPRAGMA(X) _Pragma(#X)

#define GCC_DISABLE_WARNING(WARN)                \
   _Pragma("GCC diagnostic push")                \
   DOPRAGMA(GCC diagnostic ignored WARN)           

#define GCC_ENABLE_WARNING                      \
   _Pragma("GCC diagnostic pop")

#if __GNUC__ >= 7
#define GCC7_DISABLE_WARNING(WARN) GCC_DISABLE_WARNING(WARN)
#define GCC7_ENABLE_WARNING GCC_ENABLE_WARNING
#else
#define GCC7_DISABLE_WARNING(WARN)
#define GCC7_ENABLE_WARNING
#endif

#if __GNUC__ >= 8
#define GCC8_DISABLE_WARNING(WARN) GCC_DISABLE_WARNING(WARN)
#define GCC8_ENABLE_WARNING GCC_ENABLE_WARNING
#else
#define GCC8_DISABLE_WARNING(WARN)
#define GCC8_ENABLE_WARNING
#endif

#if __GNUC__ >= 9
#define GCC9_DISABLE_WARNING(WARN) GCC_DISABLE_WARNING(WARN)
#define GCC9_ENABLE_WARNING GCC_ENABLE_WARNING
#else
#define GCC9_DISABLE_WARNING(WARN)
#define GCC9_ENABLE_WARNING
#endif

#else

#define GCC_DISABLE_WARNING(X)
#define GCC_ENABLE_WARNING

#endif

#endif
