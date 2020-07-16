pkglibexec_PROGRAMS = spindled_logd
noinst_LTLIBRARIES = libspindledlogc.la

spindled_logd_SOURCES = $(top_srcdir)/../logging/spindle_logd.cc
spindled_logd_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/../logging
spindled_logd_LDADD = -lpthread

libspindledlogc_la_SOURCES = $(top_srcdir)/../logging/spindle_logc.c
libspindledlogc_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/../logging -I$(top_srcdir)/../include
libspindledlogc_la_CFLAGS = -DLIBEXEC=\"${pkglibexecdir}\" -DDAEMON_NAME=\"spindled_logd\"

