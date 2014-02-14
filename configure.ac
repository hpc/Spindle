AC_INIT([spindle], m4_esyscmd([tr -d '\n' < VERSION]), m4_esyscmd([tr -d '\n' < BUG_EMAIL]))
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([scripts])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_SILENT_RULES([yes])
AC_PROG_CC
AC_PROG_CXX
AC_PROG_SED
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([src/client/Makefile src/server/Makefile src/fe/Makefile testsuite/Makefile Makefile])
AC_PROG_LIBTOOL
AC_PROG_LN_S
LT_INIT

m4_include([m4/ax_tls.m4])
m4_include([configure.lmon.ac])
AX_TLS

#AC_CONFIG_SUBDIRS(src/client)
#AC_CONFIG_SUBDIRS(src/server)
#AC_CONFIG_SUBDIRS(src/fe)

#Default flags
AC_SUBST([AM_CFLAGS], [-Wall])

AC_ARG_WITH(testrm,
            [AS_HELP_STRING([--with-testrm=ARG],[Resource manager to use for tests (valid options are 'slurm' and 'flux')])],
            [TESTRM=${withval}],
            [TESTRM=unknown; AC_MSG_WARN([--with-testrm not specified. Spindle tests will not work])])
AC_SUBST(TESTRM)

#Include common ops
m4_include([m4/lx_detect_bluegene.m4])
m4_include([configure.common.ac])

#MPI detection
m4_include([m4/lx_find_mpi.m4])
LX_FIND_MPI

AC_OUTPUT

m4_include([m4/runconf.m4])
RUN_CONFIG_W_COMPILER([src/client], [$CC], [$CFLAGS], [$LDFLAGS], [$LIBS], [$CPPFLAGS], [$CXX], [$CXXFLAGS], [$CPP], [$CXXCPP], [$build], [$host], [$target])
RUN_CONFIG_W_COMPILER([src/server], [$CC], [$CFLAGS], [$LDFLAGS], [$LIBS], [$CPPFLAGS], [$CXX], [$CXXFLAGS], [$CPP], [$CXXCPP], [$build], [$host], [$target])
RUN_CONFIG_W_COMPILER([src/fe], [$CC], [$CFLAGS], [$LDFLAGS], [$LIBS], [$CPPFLAGS], [$CXX], [$CXXFLAGS], [$CPP], [$CXXCPP], [$build], [$host], [$target])
