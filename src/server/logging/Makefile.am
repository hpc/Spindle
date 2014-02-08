pkglibexec_PROGRAMS = spindled_logd
noinst_LTLIBRARIES = libspindlelogc.la

spindled_logd_SOURCES = ../../logging/spindle_logd.cc
spindled_logd_CPP = -I../../logging
spindled_logd_LDADD = -lpthread

libspindlelogc_la_SOURCES = ../../logging/spindle_logc.c
libspindlelogc_la_CPP = -I../../logging
libspindlelogc_la_CFLAGS = -DLIBEXEC=${pkglibexecdir} -DDAEMON_NAME=spindled_logd
