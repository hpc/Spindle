noinst_LTLIBRARIES = libbiterd.la

AM_CFLAGS = -fvisibility=hidden

libbiterd_la_SOURCES = $(top_srcdir)/../biter/biterd.c $(top_srcdir)/../biter/server_queue.c $(top_srcdir)/../biter/demultiplex.c
libbiterd_la_CFLAGS = -DSERVER
AM_CPPFLAGS = -I$(top_srcdir)/../biter -I$(top_srcdir)/../logging -I$(top_srcdir)/../include

if BGQ_BLD
libbiterd_la_SOURCES += $(top_srcdir)/../biter/bgq_daemon_ids.c
endif
if LINUX_BLD
libbiterd_la_SOURCES += $(top_srcdir)/../biter/linux_daemon_ids.c
endif

