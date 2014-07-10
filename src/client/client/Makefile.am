noinst_LTLIBRARIES =

if SOCKETS
noinst_LTLIBRARIES += libspindlec_socket.la
endif
if PIPES
noinst_LTLIBRARIES += libspindlec_pipe.la
endif
if BITER
noinst_LTLIBRARIES += libspindlec_biter.la
endif

AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/client_comlib -I$(top_srcdir)/../include -I$(top_srcdir)/shm_cache

BASE_SRCS = client.c intercept_open.c intercept_exec.c intercept_stat.c intercept_spindleapi.c should_intercept.c exec_util.c remap_exec.c $(top_srcdir)/../utils/parseloc.c

libspindlec_socket_la_SOURCES = $(BASE_SRCS)
libspindlec_socket_la_LIBADD = $(top_builddir)/client_comlib/libclient_socket.la $(top_builddir)/logging/libspindlelogc.la $(top_builddir)/shm_cache/libshmcache.la

libspindlec_pipe_la_SOURCES = $(BASE_SRCS)
libspindlec_pipe_la_LIBADD = $(top_builddir)/client_comlib/libclient_pipe.la $(top_builddir)/logging/libspindlelogc.la $(top_builddir)/shm_cache/libshmcache.la

libspindlec_biter_la_SOURCES = $(BASE_SRCS)
libspindlec_biter_la_LIBADD = $(top_builddir)/client_comlib/libclient_biter.la $(top_builddir)/logging/libspindlelogc.la $(top_builddir)/shm_cache/libshmcache.la
