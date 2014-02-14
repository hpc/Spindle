AC_INIT([spindle], m4_esyscmd([tr -d '\n' < ../../VERSION]), m4_esyscmd([tr -d '\n' < ../../BUG_EMAIL]))
AC_CONFIG_MACRO_DIR([../../m4])
AC_CONFIG_AUX_DIR([../../scripts])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_PROG_CXX
AC_CONFIG_FILES([logging/Makefile cache/Makefile auditserver/Makefile cobo/Makefile comlib/Makefile launchmon/Makefile Makefile])
LT_INIT

#Include common ops
m4_include([../../m4/lx_detect_bluegene.m4])
m4_include([../../configure.common.ac])

#Include LaunchMON
m4_include([../../m4/lmon.m4])

#Check if we can static link libstdc++ and libgcc, which is desirable 
# for bootstrapping purposes on spindle_be.  We only do this if we
# can link everything statically: launchmon, libstdc++, libgcc.
# Otherwise we stick with dynamic linking.
if test "x$LAUNCHMON_STATIC_LIBS" != "x"; then
  #Test whether we have a static libgcc option
  AC_LANG_PUSH(C++)
  LDFLAGS_HOLD=$LDFLAGS
  LDFLAGS+=-static-libgcc
  AC_MSG_CHECKING([whether we can statically link libgcc])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([])],
                 [STATIC_LIBGCC_OPT=-static-libgcc]               
                 [AC_MSG_RESULT([yes])],
                 [AC_MSG_RESULT([no])])
  
  if test "x$STATIC_LIBGCC_OPT" != "x"; then  
    #Test whether we have a static libstdc++
    AC_MSG_CHECKING([whether we have a static libstdc++])
    STATIC_LIBSTDCXX_TEST=`$CXX -print-file-name=libstdc++.a`
    if test -f $STATIC_LIBSTDCXX_TEST; then
      STATIC_LIBSTDCXX=$STATIC_LIBSTDCXX_TEST
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
    fi
  
    if test "x$STATIC_LIBSTDCXX" != "x"; then

      AC_CHECK_PROG([LDD], [ldd], [ldd])
      if test "x$LDD" != "x"; then
        #Test whether we can link with a static libstdc++
        rm -f libstdc++.a
        $LN_S $STATIC_LIBSTDCXX
        LDFLAGS="$LDFLAGS -L."
        AC_LINK_IFELSE([AC_LANG_PROGRAM([])],
                       [STATIC_TEST_LIBS=`$LDD conftest$EXEEXT`])
        AC_MSG_CHECKING([whether we can force static linking of libstdc++])
        if [[ "$STATIC_TEST_LIBS" != *libstdc++* ]]; then
          if [[ "$STATIC_TEST_LIBS" != *libgcc* ]]; then
            LINK_LIBSTDCXX_STATIC=yes
            AC_MSG_RESULT([yes])
          else
            LINK_LIBSTDCXX_STATIC=no
            AC_MSG_RESULT([no])
          fi
        else
           LINK_LIBSTDCXX_STATIC=no 
           AC_MSG_RESULT([no])
        fi
        rm -f libstdc++.a
        LDFLAGS=$LDFLAGS_HOLD
        AC_LANG_POP
      fi
    fi
  fi
fi

AC_SUBST(STATIC_LIBGCC_OPT)
AC_SUBST(STATIC_LIBSTDCXX)
AM_CONDITIONAL([LINK_LIBSTDCXX_STATIC], [test "$LINK_LIBSTDCXX_STATIC" == "yes"])

AC_OUTPUT

