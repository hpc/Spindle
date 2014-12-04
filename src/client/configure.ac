AC_INIT([spindle], m4_esyscmd([tr -d '\n' < ../../VERSION]), m4_esyscmd([tr -d '\n' < ../../BUG_EMAIL]))
AC_CONFIG_MACRO_DIR([../../m4])
AC_CONFIG_AUX_DIR([../../scripts])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_CONFIG_FILES([logging/Makefile biter/Makefile shm_cache/Makefile client_comlib/Makefile client/Makefile auditclient/Makefile subaudit/Makefile beboot/Makefile spindle_api/Makefile ldsolookup/Makefile Makefile])
LT_INIT

#Include common ops
m4_include([../../m4/lx_detect_bluegene.m4])
m4_include([../../configure.common.ac])

AC_SUBST([LIBSPINDLE_LIB_VERSION],[`$srcdir/../../LIB_VERSION libspindle`])

AC_OUTPUT

