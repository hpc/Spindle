AC_INIT([spindle], [0.4], [legendre1@llnl.gov])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([scripts])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_SILENT_RULES([yes])
AC_PROG_CC
AC_PROG_CXX
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
                 beboot/Makefile
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
  LAUNCHMON_BIN=$LAUNCHMON_LIB_DIR/../bin
fi
LDFLAGS="$LDFLAGS $LAUNCHMON_LIB"
AC_CHECK_LIB(monfeapi, LMON_fe_init, 
             [AC_MSG_NOTICE([Found launchmon libraries])],
             [AC_ERROR(Couldn't find LaunchMON libraries)])
LDFLAGS=$LDFLAGS_HOLD
LIBS=$LIBS_HOLD

#Runmode detection (pipe/socket or cobo/msocket communications)
CLIENT_SERVER_COM=pipes
SERVER_SERVER_COM=cobo
AC_ARG_ENABLE(pipes,
              [AS_HELP_STRING([--with-pipes],[Use pipes for server/client communication])],
              [CLIENT_SERVER_COM=pipes;],)
AC_ARG_ENABLE(socket,
              [AS_HELP_STRING([--with-socket],[Use sockets for server/client communication])],
              [CLIENT_SERVER_COM=sockets;],)
AC_ARG_ENABLE(msocket,
              [AS_HELP_STRING([--with-msocket],[Use msocket for inter-server communication])]
              [SERVER_SERVER_COM=msocket;],)
AC_ARG_ENABLE(cobo,
              [AS_HELP_STRING([--with-cobo],[Use msocket for inter-server communication])]
              [SERVER_SERVER_COM=cobo;],)

AM_CONDITIONAL([PIPES], [test x$CLIENT_SERVER_COM == xpipes])
AM_CONDITIONAL([SOCKETS], [test x$CLIENT_SERVER_COM == xsocket])
AM_CONDITIONAL([MSOCKET], [test x$SERVER_SERVER_COM == xmsocket])
AM_CONDITIONAL([COBO], [test x$SERVER_SERVER_COM == xcobo])

if test "x$CLIENT_SERVER_COM" == "xpipes"; then
  AC_DEFINE([COMM_PIPES],[1],[Define if were using pipes for client/server communication])
fi
if test "x$CLIENT_SERVER_COM" == "xsocket"; then
  AC_DEFINE([COMM_SOCKET],[1],[Define if were using sockets for client/server communication])
fi
if test "x$SERVER_SERVER_COM" == "xmsocket"; then
  AC_DEFINE([COMM_MSOCKET],[1],[Define if were using msocket for server/server communication])
fi
if test "x$SERVER_SERVER_COM" == "xcobo"; then
  AC_DEFINE([COMM_COBO],[1],[Define if were using cobo for server/server communication])
fi

AC_MSG_NOTICE([Communication is $CLIENT_SERVER_COM/$SERVER_SERVER_COM])

AC_SUBST(LAUNCHMON_INC)
AC_SUBST(LAUNCHMON_LIB)
AC_DEFINE_UNQUOTED([LAUNCHMON_BIN_DIR],"[$LAUNCHMON_BIN]",[The bin directory for launchmon])
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
if (( "x$target_os" == "xlinux" || "x$target_os" == "xlinux-gnu" )); then
AC_DEFINE([os_linux],[1],[Define if we're compiling for a linux target])
fi
AC_MSG_NOTICE([os is $target_os])

AC_OUTPUT

