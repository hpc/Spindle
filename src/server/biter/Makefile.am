noinst_LTLIBRARIES = libbiterd.la

BS=$(top_srcdir)/../biter

libbiterd_la_SOURCES = $(BS)/biterd.c $(BS)/server_queue.c $(BS)/demultiplex.c
AM_CPPFLAGS = -I$(BS) -I$(top_srcdir)/../logging -I$(top_srcdir)/../include

if BGQ_BLD
libbiterd_la_SOURCES += $(BS)/bgq_daemon_ids.c
endif
if LINUX_BLD
libbiterd_la_SOURCES += $(BS)/linux_daemon_ids.c
endif

