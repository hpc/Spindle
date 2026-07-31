AC_INIT([spindle], m4_esyscmd([tr -d '\n' < ../../VERSION]), m4_esyscmd([tr -d '\n' < ../../BUG_EMAIL]))
AC_CONFIG_MACRO_DIR([../../m4])
AC_CONFIG_AUX_DIR([../../scripts])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_SILENT_RULES([yes])
AM_PROG_AR
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_CONFIG_FILES([logging/Makefile cobo/Makefile comlib/Makefile openmpi_intercept/Makefile flux/Makefile startup/Makefile hostbin/Makefile Makefile])
AC_PROG_LN_S
LT_INIT

SPINDLE_BUILD_ROOT=`pwd`/../..
SPINDLE_SOURCE_ROOT=`realpath ${srcdir}/../..`

#Include common ops
m4_include([../../configure.common.ac])
CHOOSE_SEC
CHOOSE_DEF_AUDIT

#Include RM Selection
m4_include([../../m4/launcher.m4])

CPPFLAGS="$CPPFLAGS -DSPINDLEFE"
CFLAGS="$CFLAGS -fvisibility=hidden"
CXXFLAGS="$CXXFLAGS -fvisibility=hidden"

AC_SUBST([SPINDLEFE_LIB_VERSION],[`$srcdir/../../LIB_VERSION spindlefe`])

if test "x$CLEANUP_PROC" == "xtrue"; then
  AC_DEFINE([DEFAULT_CLEANUP_PROC],[1],[Default to using a dedicated process to clean data files])
fi

AC_OUTPUT
AC_SUBST(PKGSYSCONF_DIR)
