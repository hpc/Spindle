noinst_LTLIBRARIES = libbiterc.la libsheep.la

BS=$(top_srcdir)/../biter

libbiterc_la_SOURCES = $(BS)/biterc.c $(BS)/client_queue.c $(BS)/demultiplex.c
libbiterc_la_LIBADD = libsheep.la

libsheep_la_SOURCES = $(BS)/sheep.c $(BS)/shmutil.c $(BS)/shm_wrappers.c

AM_CPPFLAGS = -I$(BS) -I$(top_srcdir)/../logging

if BGQ_BLD
AM_CPPFLAGS += -I/bgsys/drivers/ppcfloor/comm/lib/gnu -I/bgsys/drivers/ppcfloor -I/bgsys/drivers/ppcfloor/comm/sys/include -I/bgsys/drivers/ppcfloor/spi/include -I/bgsys/drivers/ppcfloor/spi/include/kernel/cnk
libbiterc_la_SOURCES += $(BS)/bgq_ids.c
endif
if LINUX_BLD
libbiterc_la_SOURCES += $(BS)/linux_ids.c
endif
