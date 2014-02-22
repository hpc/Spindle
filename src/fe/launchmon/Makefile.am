bin_PROGRAMS = spindle

AM_LDFLAGS = $(LAUNCHMON_LIB) $(LAUNCHMON_RMCOMM)
AM_CPPFLAGS = $(LAUNCHMON_INC) -I$(top_srcdir)/../logging

spindle_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/../include -I$(top_srcdir)/comlib -I$(top_srcdir)/../server/cache -I$(top_srcdir)/../server/comlib -I$(top_srcdir)/../utils -DBINDIR=\"$(pkglibexecdir)\" -DLIBEXECDIR=\"$(pkglibexecdir)\"
spindle_SOURCES = spindle_fe_main.cc spindle_fe_lmon.cc spindle_fe_serial.cc spindle_fe.cc parse_launcher.cc parseargs.cc parse_preload.cc $(top_srcdir)/../utils/pathfn.c

spindle_LDADD = $(AM_LDFLAGS) -lmonfeapi $(top_builddir)/logging/libspindlelogc.la
if COBO
spindle_LDADD += $(top_builddir)/comlib/libfe_cobo.la $(top_builddir)/cobo/libcobocomm.la $(top_builddir)/cobo/libldcs_cobo.la
endif
if MSOCKET
spindle_LDADD += $(top_builddir)/comlib/libfe_msocket.la
endif
