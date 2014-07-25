noinst_LTLIBRARIES = libspindle_audit.la libspindle_instr.la

if SOCKETS
noinst_LTLIBRARIES += libspindlec_socket.la
endif
if PIPES
noinst_LTLIBRARIES += libspindlec_pipe.la
endif
if BITER
noinst_LTLIBRARIES += libspindlec_biter.la
endif

AM_CFLAGS = -fvisibility=hidden

AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/client_comlib -I$(top_srcdir)/../include -I$(top_srcdir)/shm_cache

INTERCEPT_SRCS = intercept_open.c intercept_exec.c intercept_stat.c intercept_spindleapi.c intercept.c

BASE_SRCS = client.c should_intercept.c exec_util.c remap_exec.c $(top_srcdir)/../utils/parseloc.c

libspindlec_socket_la_SOURCES = $(BASE_SRCS)
libspindlec_socket_la_LIBADD = $(top_builddir)/client_comlib/libclient_socket.la $(top_builddir)/logging/libspindlelogc.la $(top_builddir)/shm_cache/libshmcache.la

libspindlec_pipe_la_SOURCES = $(BASE_SRCS)
libspindlec_pipe_la_LIBADD = $(top_builddir)/client_comlib/libclient_pipe.la $(top_builddir)/logging/libspindlelogc.la $(top_builddir)/shm_cache/libshmcache.la

libspindlec_biter_la_SOURCES = $(BASE_SRCS)
libspindlec_biter_la_LIBADD = $(top_builddir)/client_comlib/libclient_biter.la $(top_builddir)/logging/libspindlelogc.la $(top_builddir)/shm_cache/libshmcache.la

libspindle_audit_la_SOURCES = $(INTERCEPT_SRCS)
libspindle_audit_la_CPPFLAGS = -DAUDIT_LIB -I$(top_srcdir)/auditclient $(AM_CPPFLAGS)

libspindle_instr_la_SOURCES = $(INTERCEPT_SRCS)
libspindle_instr_la_CPPFLAGS = -DINSTR_LIB -I$(top_srcdir)/instrclient $(AM_CPPFLAGS)
