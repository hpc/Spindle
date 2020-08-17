#MPI Implementations Support

AC_ARG_ENABLE(slurm,
              [AS_HELP_STRING([--enable-slurm],[Enable support for the SLURM job launcher])],
              [ENABLE_SLURM="true",EXPLICIT_RM="true"])

AC_ARG_ENABLE(openmpi,
              [AS_HELP_STRING([--enable-openmpi],[Enable support for the OpenMPI job launcher])],
              [ENABLE_OPENMPI="true",EXPLICIT_RM="true"])

AC_ARG_ENABLE(wreck,
              [AS_HELP_STRING([--enable-wreck],[Enable support for the Wreck job launcher])],
              [ENABLE_WRECK="true",EXPLICIT_RM="true"])

AC_ARG_ENABLE(lrun,
              [AS_HELP_STRING([--enable-lrun],[Enable support for LLNL's lrun job launcher])],
              [ENABLE_LRUN="true",EXPLICIT_RM="true"])

AC_ARG_ENABLE(jsrun,
              [AS_HELP_STRING([--enable-jsrun],[Enable support for IBM's jsrun job launcher])],
              [ENABLE_JSRUN="true",EXPLICIT_RM="true"])

if test "x$TESTRM" != "xunknown"; then
  EXPLICIT_RM="true"
fi

if test "x$EXPLICIT_RM" != "xtrue"; then
   ENABLE_SLURM="true"
   ENABLE_OPENMPI="true"
   ENABLE_WRECK="true"
   ENABLE_LRUN="true"
   ENABLE_JSRUN="true"
fi

if test "x$TESTRM" == "xslurm"; then
   ENABLE_SLURM="true"
fi
if test "x$TESTRM" == "xflux"; then
   ENABLE_WRECK="true"
fi
if test "x$TESTRM" == "xlrun"; then
   ENABLE_LRUN="true"
fi
if test "x$TESTRM" == "xjsrun"; then
   ENABLE_JSRUN="true"
fi

if test "x$ENABLE_LRUN" == "xtrue"; then
   ENABLE_JSRUN="true"
fi

if test "x$ENABLE_SLURM" == "xtrue"; then
   AC_DEFINE([ENABLE_SRUN_LAUNCHER],[1],[Enable support for srun])
fi
if test "x$ENABLE_OPENMPI" == "xtrue"; then
   AC_DEFINE([ENABLE_OPENMPI_LAUNCHER],[1],[Enable support for openmpi])
fi
if test "x$ENABLE_WRECK" == "xtrue"; then
   AC_DEFINE([ENABLE_WRECKRUN_LAUNCHER],[1],[Enable support for wreckrun])
fi
if test "x$ENABLE_LRUN" == "xtrue"; then
   AC_DEFINE([ENABLE_LRUN_LAUNCHER],[1],[Enable support for lrun])
fi
if test "x$ENABLE_JSRUN" == "xtrue"; then
   AC_DEFINE([ENABLE_JSRUN_LAUNCHER],[1],[Enable support for jsrun])
fi

