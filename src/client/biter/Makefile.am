noinst_LTLIBRARIES = libbiterc.la libsheep.la

libbiterc_la_SOURCES = $(top_srcdir)/../biter/biterc.c $(top_srcdir)/../biter/client_queue.c $(top_srcdir)/../biter/demultiplex.c
libbiterc_la_CFLAGS = -DCLIENT
libbiterc_la_LIBADD = libsheep.la

libsheep_la_SOURCES = $(top_srcdir)/../biter/sheep.c $(top_srcdir)/../biter/shmutil.c $(top_srcdir)/../biter/shm_wrappers.c

AM_CPPFLAGS = -I$(top_srcdir)/../biter -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -I$(top_srcdir)/../utils
AM_CFLAGS = -fvisibility=hidden

libbiterc_la_SOURCES += $(top_srcdir)/../biter/linux_ids.c
