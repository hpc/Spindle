pkglibexec_LTLIBRARIES = 

if SOCKETS
pkglibexec_LTLIBRARIES += libspindle_client_socket.la
endif
if PIPES
pkglibexec_LTLIBRARIES += libspindle_client_pipe.la
endif

AM_CPPFLAGS = -I$(top_srcdir)/logging -I$(top_srcdir)/client_comlib -I$(top_srcdir)/comlib -I$(top_srcdir)/launchmon

BASE_SOURCES = client.c auditclient.c intercept_open.c intercept_exec.c intercept_stat.c intercept_spindleapi.c patch_linkmap.c should_intercept.c exec_util.c
ARCH_SOURCES = auditclient_x86_64.c

libspindle_client_socket_la_SOURCES = $(BASE_SOURCES) $(ARCH_SOURCES)
libspindle_client_socket_la_LIBADD = $(top_builddir)/client_comlib/libclient_socket.la $(top_builddir)/logging/libspindlelogc.la
libspindle_client_socket_la_LDFLAGS = -shared

libspindle_client_pipe_la_SOURCES = $(BASE_SOURCES) $(ARCH_SOURCES)
libspindle_client_pipe_la_LIBADD = $(top_builddir)/client_comlib/libclient_pipe.la $(top_builddir)/logging/libspindlelogc.la
libspindle_client_pipe_la_LDFLAGS = -shared
