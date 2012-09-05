#################################################################################################
# Copyright (c) 2010, Lawrence Livermore National Security, LLC.
# Produced at the Lawrence Livermore National Laboratory
# Written by Todd Gamblin, tgamblin@llnl.gov.
# LLNL-CODE-417602
# All rights reserved.
#
# This file is part of Libra. For details, see http://github.com/tgamblin/libra.
# Please also read the LICENSE file for further information.
#
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this list of
# conditions and the disclaimer below.
# * Redistributions in binary form must reproduce the above copyright notice, this list of
# conditions and the disclaimer (as noted below) in the documentation and/or other materials
# provided with the distribution.
# * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
# or promote products derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#################################################################################################

#
# LX_DETECT_BLUEGENE
#
# Defines some compile-time tests to detect BlueGene architectures.
# This will AC_DEFINE the following macros if detected:
#
# BLUEGENE_L If BlueGene/L architecture is detected
# BLUEGENE_P If BlueGene/P architecture is detected
# BLUEGENE_Q If BlueGene/Q architecture is detected
#
AC_DEFUN([LX_DETECT_BLUEGENE],
[
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#ifndef __blrts__
#error "not BlueGene/L"
#endif /* __blrts__ */
]])],
  [
   AC_DEFINE([os_bgl],[1],[Define if we're compiling for a BlueGene/L system])
   AC_DEFINE([os_bg],[1],[Define if we're compiling for any BlueGene system])
  ])

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#ifdef __linux__
#include <linux/config.h>
#endif /* __linux__ */
#if !(defined(__bgp__) || defined(CONFIG_BGP))
#error "not BlueGene/P"
#endif /* !(defined(__bgp__) || defined(CONFIG_BGP)) */
]])],
  [
   AC_DEFINE([os_bgp],[1],[Define if we're compiling for a BlueGene/P system])
   AC_DEFINE([os_bg],[1],[Define if we're compiling for any BlueGene system])
  ])

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if !defined(__bgq__)
#error "not BlueGene/Q"
#endif
]])],
  [
   AC_DEFINE([os_bgq],[1],[Define if we're compiling for a BlueGene/Q system])
   AC_DEFINE([os_bg],[1],[Define if we're compiling for any BlueGene system])
  ])

])
