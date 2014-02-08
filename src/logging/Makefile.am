pkglibexec_PROGRAMS = spindle_logd
noinst_LTLIBRARIES = libspindlelogc.la

spindle_logd_SOURCES = spindle_logd.cc
spindle_logd_LDADD = -lpthread

libspindlelogc_la_SOURCES = spindle_logc.c

libspindlelogc_la_CFLAGS = -DLIBEXEC=${pkglibexecdir}
