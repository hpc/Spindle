#Launchmon processing
AC_ARG_WITH(launchmon,
        [AS_HELP_STRING([--with-launchmon=DIR],[Launchmon directory (must contain ./lib and ./include)])],
        [LAUNCHMON_INC_DIR="${withval}/include";
        LAUNCHMON_LIB_DIR="${withval}/lib";
        LAUNCHMON_RMCOMM_DIR="${withval}/lib"
        EXPLICIT_LMON=true],)
AC_ARG_WITH(launchmon-incdir,
            [AS_HELP_STRING([--with-launchmon-incdir=DIR],[Launchmon include directory])],
            [LAUNCHMON_INC_DIR="${withval}";EXPLICIT_LMON=true],)
AC_ARG_WITH(launchmon-libdir,
            [AS_HELP_STRING([--with-launchmon-libdir=DIR],[Launchmon library directory])],
            [LAUNCHMON_LIB_DIR="${withval}";EXPLICIT_LMON=true],)
AC_ARG_WITH(launchmon-rmcommdir,
            [AS_HELP_STRING([--with-launchmon-rmcommdir=DIR],[RM communication library LaunchMON uses])],
            [LAUNCHMON_RMCOMM_DIR="${withval}";EXPLICIT_LMON=true],)

CPPFLAGS_HOLD=$CPPFLAGS
if test "x$LAUNCHMON_INC_DIR" != "x"; then
  LAUNCHMON_INC=-I$LAUNCHMON_INC_DIR
fi
CPPFLAGS="$CPPFLAGS $LAUNCHMON_INC"
AC_CHECK_HEADER([lmon_api/lmon_fe.h], 
                [AC_MSG_NOTICE([Found launchmon headers])],
                [if test "x$EXPLICIT_LMON" == "xtrue"; 
                   then AC_MSG_ERROR([Could not find or build LaunchMON includes])
                   else AC_MSG_WARN([Could not find or build LaunchMON includes]); HAVE_LMON=false
                 fi])
CPPFLAGS=$CPPFLAGS_HOLD

if test "xHAVE_LMON" != "xfalse"; then
  LDFLAGS_HOLD=$LDFLAGS
  LIBS_HOLD=$LIBS
  if test "x$LAUNCHMON_LIB_DIR" != "x"; then
    LAUNCHMON_LIB=-L$LAUNCHMON_LIB_DIR
    LAUNCHMON_BIN=$LAUNCHMON_LIB_DIR/../bin
  fi
  LDFLAGS="$LDFLAGS $LAUNCHMON_LIB"

  if test "x$LAUNCHMON_RMCOMM_DIR" != "x"; then
    LAUNCHMON_RMCOMM="-Wl,-rpath=$LAUNCHMON_RMCOMM_DIR"
  fi 

  AC_CHECK_LIB(monfeapi, LMON_fe_init, 
               [AC_MSG_NOTICE([Found launchmon libraries]);HAVE_LMON=true],
               [if test "x$EXPLICIT_LMON" == "xtrue"; 
                   then AC_MSG_ERROR([Could not find or build LaunchMON libraries])
                   else AC_MSG_WARN([Could not find or build LaunchMON libraries]); HAVE_LMON=false
                fi])
fi

if test "xHAVE_LMON" != "xfalse"; then
  AC_LANG_PUSH(C++)
  AC_MSG_CHECKING([whether we can statically link launchmon])
  LAUNCHMON_STATIC_LIBS_TEST="$LAUNCHMON_LIB_DIR/libmonbeapi.a $LAUNCHMON_LIB_DIR/libcobo.a $LAUNCHMON_LIB_DIR/libgcrypt.a $LAUNCHMON_LIB_DIR/libgpg-error.a"
  LIBS="$LIBS $LAUNCHMON_STATIC_LIBS_TEST"
  AC_LINK_IFELSE(AC_LANG_PROGRAM([extern "C" { extern int LMON_be_init(); } ],[return LMON_be_init();]),
                 [LAUNCHMON_STATIC_LIBS=$LAUNCHMON_STATIC_LIBS_TEST]
                 AC_MSG_RESULT([yes]),
                 [])
  if test "x$LAUNCHMON_STATIC_LIBS x$HAVE_MUNGE" == "x xtrue"; then
    LIBS="$LIBS_HOLD $MUNGE_STATIC_LIB"
    AC_LINK_IFELSE(AC_LANG_PROGRAM([extern "C" { extern int LMON_be_init(); } ],[return LMON_be_init();]),
                   [LAUNCHMON_STATIC_LIBS=$LAUNCHMON_STATIC_LIBS_TEST $MUNGE_STATIC_LIB]
                    AC_MSG_RESULT([yes]),
                   [AC_MSG_RESULT([no])])
  fi
  AC_LANG_POP
  LDFLAGS=$LDFLAGS_HOLD
  LIBS=$LIBS_HOLD
fi

AC_SUBST(LAUNCHMON_STATIC_LIBS)
AC_SUBST(HAVE_LMON)

AC_SUBST(LAUNCHMON_INC)
AC_SUBST(LAUNCHMON_LIB)
AC_SUBST(LAUNCHMON_RMCOMM)
AC_DEFINE_UNQUOTED([LAUNCHMON_BIN_DIR],"[$LAUNCHMON_BIN]",[The bin directory for launchmon])

AM_CONDITIONAL([LMON], [test "x$HAVE_LMON" == "xtrue"])
AM_CONDITIONAL([LMON_DYNAMIC], [test "x$LAUNCHMON_STATIC_LIBS" == "x"])

if test "x$HAVE_LMON" == "xtrue"; then
  AC_DEFINE([HAVE_LMON], [1], [Build with LaunchMON support])
fi
