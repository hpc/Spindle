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
     -build | --build | --buil | --bui | --bu)
      ac_prev=build ;;
     --build=* | --buil=* | --bui=* | --bu=*)
      ;;
     -target | --target | --targe | --targ | --tar | --ta | --t)
      ac_prev=target ;;
     --target=* | --targe=* | --targ=* | --tar=* | --ta=* | --t=*)
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

   ac_sub_configure_args="$ac_sub_configure_args 'CC=$2' 'CFLAGS=$3' 'LDFLAGS=$4' 'LIBS=$5' 'CPPFLAGS=$6' 'CXX=$7' 'CXXFLAGS=$8' 'CPP=$9' 'CXXCPP=$10' '--build=$11' '--host=$12' '--target=$13'"

   eval "\$SHELL \"$conf_cmd\" $ac_sub_configure_args"
   cd $orig_dir])
