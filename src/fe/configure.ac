AC_INIT([spindle], m4_esyscmd([tr -d '\n' < ../../VERSION]), m4_esyscmd([tr -d '\n' < ../../BUG_EMAIL]))
AC_CONFIG_MACRO_DIR([../../m4])
AC_CONFIG_AUX_DIR([../../scripts])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_PROG_CXX
AC_CONFIG_FILES([logging/Makefile cobo/Makefile comlib/Makefile openmpi_intercept/Makefile launchmon/Makefile startup/Makefile hostbin/Makefile Makefile])
LT_INIT

#Include common ops
m4_include([../../m4/lx_detect_bluegene.m4])
m4_include([../../configure.common.ac])
CHOOSE_SEC

#Include LaunchMON
m4_include([../../m4/lmon.m4])
#Include RM Selection
m4_include([../../m4/launcher.m4])

CPPFLAGS="$CPPFLAGS -DSPINDLEFE"
CFLAGS="$CFLAGS -fvisibility=hidden"
CXXFLAGS="$CXXFLAGS -fvisibility=hidden"

if test "x$SPAWNNET_DIR" != "x"; then
   CFLAGS="$CFLAGS -I$SPAWNNET_DIR/include"
   CXXFLAGS="$CFLAGS -I$SPAWNNET_DIR/include"
   LDFLAGS="$LDFLAGS -L$SPAWNNET_DIR/lib"
fi

AC_SUBST([SPINDLEFE_LIB_VERSION],[`$srcdir/../../LIB_VERSION spindlefe`])

AC_OUTPUT

