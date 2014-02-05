noinst_LTLIBRARIES = libbiter.la

libbiter_la_SOURCES = biter_shm.c biterc.c biterd.c client_queue.c demultiplex.c server_queue.c sheep.c
if BGQ_BLD
libbiter_la_SOURCES += bgq_daemon_ids.c bgq_ids.c
endif
if LINUX_BLD
libbiter_la_SOURCES += linux_daemon_ids.c linux_ids.c
endif

