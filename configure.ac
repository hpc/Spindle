AC_INIT([spindle], [0.4], [legendre1@llnl.gov])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([scripts])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_SILENT_RULES([yes])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile
                 tools/sion_debug/Makefile
                 comlib/Makefile
                 auditclient/Makefile
                 cache/Makefile
                 tools/cobo/src/Makefile
                 auditserver/Makefile
                 client/Makefile
                 launchmon/Makefile
                 simulator/Makefile
                 tools/cobo/test/Makefile])
AC_PROG_LIBTOOL
AC_PROG_RANLIB

#Default flags
AC_SUBST([AM_CFLAGS], [-Wall])

#Launchmon processing
AC_ARG_WITH(launchmon,
        [AS_HELP_STRING([--with-launchmon],[Launchmon directory (must contain ./lib and ./include)])],
        [LAUNCHMON_INC_DIR="${withval}/include";
        LAUNCHMON_LIB_DIR="${withval}/lib";],)
AC_ARG_WITH(launchmon-incdir,
            [AS_HELP_STRING([--with-launchmon-incdir],[Launchmon include directory])],
            [LAUNCHMON_INC_DIR="${withval}";],)
AC_ARG_WITH(launchmon-libdir,
            [AS_HELP_STRING([--with-launchmon-libdir],[Launchmon library directory])],
            [LAUNCHMON_LIB_DIR="${withval}";],)

CPPFLAGS_HOLD=$CPPFLAGS
if test "x$LAUNCHMON_INC_DIR" != "x"; then
  LAUNCHMON_INC=-I$LAUNCHMON_INC_DIR
fi
CPPFLAGS="$CPPFLAGS $LAUNCHMON_INC"
AC_CHECK_HEADER([lmon_api/lmon_fe.h], 
                [AC_MSG_NOTICE([Found launchmon headers])],
                [AC_ERROR(Couldn't find or build LaunchMON includes)])
CPPFLAGS=$CPPFLAGS_HOLD

LDFLAGS_HOLD=$LDFLAGS
LIBS_HOLD=$LIBS
if test "x$LAUNCHMON_LIB_DIR" != "x"; then
  LAUNCHMON_LIB=-L$LAUNCHMON_LIB_DIR
fi
LDFLAGS="$LDFLAGS $LAUNCHMON_LIB"
AC_CHECK_LIB(monfeapi, LMON_fe_init, 
             [AC_MSG_NOTICE([Found launchmon libraries])],
             [AC_ERROR(Couldn't find LaunchMON libraries)])
LDFLAGS=$LDFLAGS_HOLD
LIBS=$LIBS_HOLD

AC_SUBST(LAUNCHMON_INC)
AC_SUBST(LAUNCHMON_LIB)

#Architecture detection
if test "x$target_cpu" == "xx86_64"; then
  AC_DEFINE([arch_x86_64],[1],[Define if we're compiling for an x86_64 target])
fi
if test "x$target_cpu" == "xppc64"; then
  AC_DEFINE([arch_ppc64],[1],[Define if we're compiling for a ppc64 target])
fi
if test "x$target_cpu" == "xpowerpc64"; then
  AC_DEFINE([arch_ppc64],[1],[Define if we're compiling for a ppc64 target])
fi
if test "x$target_cpu" == "xppc32"; then
  AC_DEFINE([arch_ppc32],[1],[Define if we're compiling for a ppc64 target])
fi
if test "x$target_cpu" == "xpowerpc32"; then
  AC_DEFINE([arch_ppc32],[1],[Define if we're compiling for a ppc64 target])
fi

#MPI detection
m4_include([m4/lx_find_mpi.m4])
LX_FIND_MPI

#BlueGene detection
if test "x$target_os" == "xcnl"; then
m4_include([m4/lx_detect_bluegene.m4])
CC_HOLD=$CC
CC=$MPICC
LX_DETECT_BLUEGENE
CC=$CC_HOLD
fi

#Linux detection
if test "x$target_os" == "xlinux"; then
AC_DEFINE([os_linux],[1],[Define if we're compiling for a linux target])
fi

AC_OUTPUT

