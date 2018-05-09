bin_PROGRAMS = spindle
lib_LTLIBRARIES = libspindlefe.la
include_HEADERS = $(top_srcdir)/../include/spindle_launch.h

AM_CPPFLAGS = -I$(top_srcdir)/../logging

CORE_SOURCES = spindle_fe.cc parseargs.cc parse_preload.cc $(top_srcdir)/../utils/pathfn.c $(top_srcdir)/../utils/keyfile.c $(top_srcdir)/../utils/parseloc.c
CORE_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/../include -I$(top_srcdir)/comlib -I$(top_srcdir)/../server/cache -I$(top_srcdir)/../server/comlib -I$(top_srcdir)/../utils -I$(top_srcdir)/../cobo -DBINDIR=\"$(installlibexecdir)\" -DLIBEXECDIR=\"$(installlibexecdir)\" -DPROGLIBDIR=\"$(installlibdir)\"
CORE_LDADD = $(top_builddir)/logging/libspindlelogc.la -lpthread
if COBO
CORE_LDADD += $(top_builddir)/comlib/libfe_cobo.la $(top_builddir)/cobo/libldcs_cobo.la
endif
if MSOCKET
CORE_LDADD += $(top_builddir)/comlib/libfe_msocket.la
endif
CORE_LDADD += $(MUNGE_DYN_LIB) $(GCRYPT_LIBS)

libspindlefe_la_CPPFLAGS = $(CORE_CPPFLAGS) -DSPINDLEFELIB
libspindlefe_la_SOURCES = $(CORE_SOURCES)
libspindlefe_la_LIBADD = $(CORE_LDADD)
libspindlefe_la_LDFLAGS = -version-info $(SPINDLEFE_LIB_VERSION)

spindle_CPPFLAGS = $(CORE_CPPFLAGS) -DSPINDLEEXE
spindle_SOURCES = spindle_fe_main.cc spindle_fe_serial.cc parse_launcher.cc parse_launcher_args.cc $(CORE_SOURCES)

spindle_LDADD = $(CORE_LDADD) $(top_builddir)/openmpi_intercept/libparseompi.la $(top_builddir)/hostbin/libhostbin.la

if LMON
spindle_LDADD += $(top_builddir)/launchmon/libfelmon.la
endif

