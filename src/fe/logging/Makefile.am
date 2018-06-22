pkglibexec_PROGRAMS = spindlef_logd
noinst_LTLIBRARIES = libspindleflogc.la

spindlef_logd_SOURCES = $(top_srcdir)/../logging/spindle_logd.cc
spindlef_logd_CPPFLAGS = -I$(top_srcdir)/../logging
spindlef_logd_LDADD = -lpthread

libspindleflogc_la_SOURCES = $(top_srcdir)/../logging/spindle_logc.c $(top_srcdir)/../utils/spindle_mkdir.c
libspindleflogc_la_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../utils -I$(top_srcdir)/../include
libspindleflogc_la_CFLAGS = -DLIBEXEC=\"${pkglibexecdir}\" -DDAEMON_NAME=\"spindlef_logd\"
