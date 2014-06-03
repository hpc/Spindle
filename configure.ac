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
AC_CONFIG_FILES([doc/Makefile testsuite/Makefile Makefile])
AC_PROG_LIBTOOL
AC_PROG_LN_S
LT_INIT

#Include common ops
m4_include([configure.common.ac])
m4_include([m4/ax_tls.m4])
m4_include([m4/lmon.m4])
AX_TLS

#Default flags
AC_SUBST([AM_CFLAGS], [-Wall])

AC_ARG_WITH(testrm,
            [AS_HELP_STRING([--with-testrm=ARG],[Resource manager to use for tests (valid options are 'slurm' and 'flux')])],
            [TESTRM=${withval}],
            [TESTRM=unknown; AC_MSG_WARN([--with-testrm not specified. Spindle tests will not work])])
AC_SUBST(TESTRM)

#MPI detection
m4_include([m4/lx_find_mpi.m4])
AC_LANG_PUSH(C)
LX_FIND_MPI
AC_LANG(C++)
LX_FIND_MPI
AC_LANG_POP

AC_ARG_VAR([BE_CC], [CC for back end])
AC_ARG_VAR([BE_CFLAGS], [CFLAGS back end])
AC_ARG_VAR([BE_LDFLAGS], [LDFLAGS for back end])
AC_ARG_VAR([BE_LIBS], [LIBS for back end])
AC_ARG_VAR([BE_CPPFLAGS], [CPPFLAGS for back end])
AC_ARG_VAR([BE_CXX], [CXX for back end])
AC_ARG_VAR([BE_CXXFLAGS], [CXXFLAGS for back end])
AC_ARG_VAR([BE_CPP], [CPP for back end])
AC_ARG_VAR([BE_CXXCPP], [CXXCPP for back end])
AC_ARG_VAR([BE_host], [host to use for back end])

AC_OUTPUT

#Set BE_CC, BE_CXX and related flags
AC_DEFUN([SET_BE_COMPILER_VAR],[
  VARNAME=\$BE_$1
  BE_VAR=`eval echo $VARNAME`
  if test "x$OS_BUILD" == "xbluegene"; then
     if test "x$BE_VAR" != "x"; then
       CLIENT_$1=$BE_VAR
     elif test "x$2" != "x"; then
       CLIENT_$1=$2
     elif test "x$USER_SPECIFIED_COMPS" == "xtrue"; then
       echo Setting CLIENT_$1 to $$1
       CLIENT_$1=
     else
       CLIENT_$1=$$1
     fi
     SERVER_$1=$$1
     FE_$1=$$1
  elif test "x$OS_BUILD" == "xlinux"; then
     if test "x$BE_VAR" == "x"; then
       if test "x$USER_SPECIFIED_COMPS" == "x"; then
         BE_VAR=$$1
       fi
     fi
     CLIENT_$1=$BE_VAR
     SERVER_$1=$BE_VAR
     FE_$1=$$1
  fi])

if test "$BE_CC x $BE_CXX x $BE_host" != " x  x "; then
   USER_SPECIFIED_COMPS="true"
fi

if test "x$OS_BUILD" == "xbluegene"; then
  if test -d /bgsys/drivers/toolchain/V1R2M1/gnu-linux/bin; then
     BG_COMP_PATH=/bgsys/drivers/toolchain/V1R2M1/gnu-linux/bin/
  elif test -d /bgsys/drivers/toolchain/V1R2M0/gnu-linux/bin; then
     BG_COMP_PATH=/bgsys/drivers/toolchain/V1R2M0/gnu-linux/bin/
  fi
fi

SET_BE_COMPILER_VAR([CC], [${BG_COMP_PATH}powerpc64-bgq-linux-gcc])
SET_BE_COMPILER_VAR([CFLAGS])
SET_BE_COMPILER_VAR([LDFLAGS])
SET_BE_COMPILER_VAR([LIBS])
SET_BE_COMPILER_VAR([CPPFLAGS])
SET_BE_COMPILER_VAR([CXX], [${BG_COMP_PATH}powerpc64-bgq-linux-c++])
SET_BE_COMPILER_VAR([CXXFLAGS])
SET_BE_COMPILER_VAR([CPP], [${BG_COMP_PATH}powerpc64-bgq-linux-cpp])
SET_BE_COMPILER_VAR([CXXCPP])
SET_BE_COMPILER_VAR([STRIP], [${BG_COMP_PATH}powerpc64-bgq-linux-strip])
SET_BE_COMPILER_VAR([AR], [${BG_COMP_PATH}powerpc64-bgq-linux-ar])
SET_BE_COMPILER_VAR([host], [powerpc64-bgq-linux])
m4_include([m4/runconf.m4]) 
RUN_CONFIG_W_COMPILER([src/client], [$CLIENT_CC], [$CLIENT_CFLAGS], [$CLIENT_LDFLAGS], [$CLIENT_LIBS], [$CLIENT_CPPFLAGS], [$CLIENT_CXX], [$CLIENT_CXXFLAGS], [$CLIENT_CPP], [$CLIENT_CXXCPP], [$CLIENT_STRIP], [$CLIENT_AR], [$CLIENT_host])

RUN_CONFIG_W_COMPILER([src/server], [$SERVER_CC], [$SERVER_CFLAGS], [$SERVER_LDFLAGS], [$SERVER_LIBS], [$SERVER_CPPFLAGS], [$SERVER_CXX], [$SERVER_CXXFLAGS], [$SERVER_CPP], [$SERVER_CXXCPP], [$SERVER_STRIP], [$SERVER_AR], [$SERVER_host])

RUN_CONFIG_W_COMPILER([src/fe], [$FE_CC], [$FE_CFLAGS], [$FE_LDFLAGS], [$FE_LIBS], [$FE_CPPFLAGS], [$FE_CXX], [$FE_CXXFLAGS], [$FE_CPP], [$FE_CXXCPP], [$FE_STRIP], [$FE_AR], [$FE_host])

