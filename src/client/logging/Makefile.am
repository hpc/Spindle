noinst_LTLIBRARIES = libspindlelogc.la

AM_CFLAGS = -fvisibility=hidden

libspindlelogc_la_SOURCES = $(top_srcdir)/../logging/spindle_logc.c $(top_srcdir)/../utils/spindle_mkdir.c
libspindlelogc_la_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -DSPINDLECLIENT
libspindlelogc_la_CFLAGS = -DLIBEXEC=${pkglibexecdir} -DDAEMON_NAME=spindled_logd
