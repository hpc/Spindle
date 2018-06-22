noinst_LTLIBRARIES = libbiterc.la libsheep.la

libbiterc_la_SOURCES = $(top_srcdir)/../biter/biterc.c $(top_srcdir)/../biter/client_queue.c $(top_srcdir)/../biter/demultiplex.c
libbiterc_la_CFLAGS = -DCLIENT
libbiterc_la_LIBADD = libsheep.la

libsheep_la_SOURCES = $(top_srcdir)/../biter/sheep.c $(top_srcdir)/../biter/shmutil.c $(top_srcdir)/../biter/shm_wrappers.c

AM_CPPFLAGS = -I$(top_srcdir)/../biter -I$(top_srcdir)/../logging
AM_CFLAGS = -fvisibility=hidden

if BGQ_BLD
AM_CPPFLAGS += -I/bgsys/drivers/ppcfloor/comm/lib/gnu -I/bgsys/drivers/ppcfloor -I/bgsys/drivers/ppcfloor/comm/sys/include -I/bgsys/drivers/ppcfloor/spi/include -I/bgsys/drivers/ppcfloor/spi/include/kernel/cnk
libbiterc_la_SOURCES += $(top_srcdir)/../biter/bgq_ids.c
endif
if LINUX_BLD
libbiterc_la_SOURCES += $(top_srcdir)/../biter/linux_ids.c
endif
