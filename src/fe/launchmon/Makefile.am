noinst_LTLIBRARIES = libfelmon.la

libfelmon_la_SOURCES = spindle_fe_lmon.cc
libfelmon_la_CPPFLAGS = $(LAUNCHMON_INC) -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -DBINDIR=\"$(installlibexecdir)\" -DLIBEXECDIR=\"$(installlibexecdir)\"
libfelmon_la_LIBADD = $(LAUNCHMON_LIB) $(LAUNCHMON_RMCOMM) -lmonfeapi

