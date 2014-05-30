noinst_LTLIBRARIES = libshmcache.la

libshmcache_la_SOURCES = shmcache.c
if BITER
else
libshmcache_la_LIBADD = $(top_builddir)/biter/libsheep.la
endif
AM_CPPFLAGS = -I$(top_srcdir)/../biter -I$(top_srcdir)/shm_cache -I$(top_srcdir)/../logging -I$(top_srcdir)/client_comlib

