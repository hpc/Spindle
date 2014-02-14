AC_DEFUN([APPEND_TO_ARGS], [
  if test "x$2" != "x"; then
    ac_sub_configure_args="$ac_sub_configure_args '$1=$2'"
  fi
])

AC_DEFUN([RUN_CONFIG_W_COMPILER], [
   #
   # The following argument parsing code, except for where we remove/add
   # compiler options, is copied from autoconf's _AC_OUTPUT_SUBDIRS macro
   # That code is GPL v3, which makes this file the same.
   #

   # Remove --cache-file, --srcdir, and --disable-option-checking arguments
   # so they do not pile up.
   ac_sub_configure_args=
   ac_prev=
   eval "set x $ac_configure_args"
   shift
   for ac_arg
   do
     if test -n "$ac_prev"; then
       ac_prev=
       continue
     fi
     case $ac_arg in
     -cache-file | --cache-file | --cache-fil | --cache-fi \
     | --cache-f | --cache- | --cache | --cach | --cac | --ca | --c)
       ac_prev=cache_file ;;
     -cache-file=* | --cache-file=* | --cache-fil=* | --cache-fi=* \
     | --cache-f=* | --cache-=* | --cache=* | --cach=* | --cac=* | --ca=* \
     | --c=*)
       ;;
     --config-cache | -C)
       ;;
     -srcdir | --srcdir | --srcdi | --srcd | --src | --sr)
       ac_prev=srcdir ;;
     -srcdir=* | --srcdir=* | --srcdi=* | --srcd=* | --src=* | --sr=*)
       ;;
     -prefix | --prefix | --prefi | --pref | --pre | --pr | --p)
       ac_prev=prefix ;;
     -prefix=* | --prefix=* | --prefi=* | --pref=* | --pre=* | --pr=* | --p=*)
       ;;
     --disable-option-checking)
       ;;
     CC=*)
       ;;
     CFLAGS=*)
       ;;
     LDFLAGS=*)
       ;;
     LIBS=*)
       ;;
     CPPFLAGS=*)
       ;;
     CXX=*)
       ;;
     CXXFLAGS=*)
       ;;
     CPP=*)
       ;;
     CXXCPP=*)
       ;;
     STRIP=*)
       ;;
     AR=*)
       ;;
     -host | --host | --hos | --ho)
      ac_prev=host ;;
     --host=* | --hos=* | --ho=*)
      ;;
     *)
       case $ac_arg in
       *\'*) ac_arg=`AS_ECHO(["$ac_arg"]) | sed "s/'/'\\\\\\\\''/g"` ;;
       esac
       ac_sub_configure_args="$ac_sub_configure_args '$ac_arg'"
      esac
   done

   # Always prepend --prefix to ensure using the same prefix
   # in subdir configurations.
   ac_arg="--prefix=$prefix"
   case $ac_arg in
   *\'*) ac_arg=`AS_ECHO(["$ac_arg"]) | sed "s/'/'\\\\\\\\''/g"` ;;
   esac
   ac_sub_configure_args="'$ac_arg' $ac_sub_configure_args"
 
   # Pass --silent
   if test "$silent" = yes; then
     ac_sub_configure_args="--silent $ac_sub_configure_args"
   fi

   # Always prepend --disable-option-checking to silence warnings, since
   # different subdirs can have different --enable and --with options.
   ac_sub_configure_args="--disable-option-checking $ac_sub_configure_args"

   orig_dir=`pwd`
   cd $srcdir
   abs_srcdir=`pwd -P`
   cd $orig_dir/$1

   conf_cmd=$abs_srcdir/$1/configure

   echo "=== configuring in $1"

   APPEND_TO_ARGS([CC], [$2])
   APPEND_TO_ARGS([CFLAGS], [$3])
   APPEND_TO_ARGS([LDFLAGS], [$4])
   APPEND_TO_ARGS([LIBS], [$5])
   APPEND_TO_ARGS([CPPFLAGS], [$6])
   APPEND_TO_ARGS([CXX], [$7])
   APPEND_TO_ARGS([CXXFLAGS], [$8])
   APPEND_TO_ARGS([CPP], [$9])
   APPEND_TO_ARGS([CXXCPP], [$10])
   APPEND_TO_ARGS([STRIP], [$11])
   APPEND_TO_ARGS([AR], [$12])
   if test "x$13" != "x$build"; then
     ac_sub_configure_args="$ac_sub_configure_args --build=$build --host=$13"
   fi

   eval "\$SHELL \"$conf_cmd\" $ac_sub_configure_args" || AC_MSG_ERROR([configure failed in $srcdir])
   cd $orig_dir])
