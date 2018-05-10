pkglibexec_PROGRAMS = spindlef_logd
noinst_LTLIBRARIES = libspindlelogc.la

spindlef_logd_SOURCES = $(top_srcdir)/../logging/spindle_logd.cc
spindlef_logd_CPPFLAGS = -I$(top_srcdir)/../logging
spindlef_logd_LDADD = -lpthread

libspindlelogc_la_SOURCES = $(top_srcdir)/../logging/spindle_logc.c $(top_srcdir)/../utils/spindle_mkdir.c
libspindlelogc_la_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../utils -I$(top_srcdir)/../include
libspindlelogc_la_CFLAGS = -DLIBEXEC=\"${pkglibexecdir}\" -DDAEMON_NAME=\"spindlef_logd\"
