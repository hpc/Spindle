noinst_LTLIBRARIES = libspindleclogc.la

AM_CFLAGS = -fvisibility=hidden

libspindleclogc_la_SOURCES = $(top_srcdir)/../logging/spindle_logc.c $(top_srcdir)/../utils/spindle_mkdir.c
libspindleclogc_la_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -DSPINDLECLIENT
libspindleclogc_la_CFLAGS = -DLIBEXEC=\"${pkglibexecdir}\" -DDAEMON_NAME=\"spindled_logd\" $(AM_CFLAGS)
