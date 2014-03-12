pkglibexec_PROGRAMS = spindlef_logd
noinst_LTLIBRARIES = libspindlelogc.la

spindlef_logd_SOURCES = ../../logging/spindle_logd.cc
spindlef_logd_CPP = -I../../logging
spindlef_logd_LDADD = -lpthread

libspindlelogc_la_SOURCES = ../../logging/spindle_logc.c
libspindlelogc_la_CPP = -I../../logging
libspindlelogc_la_CFLAGS = -DLIBEXEC=${pkglibexecdir} -DDAEMON_NAME=spindlef_logd
