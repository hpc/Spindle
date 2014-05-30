noinst_LTLIBRARIES =

AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include

BASE_SRCS = client_api.c client_heap.c client_wrappers.c

noinst_LTLIBRARIES += libclient_pipe.la
libclient_pipe_la_CPPFLAGS = $(AM_CPPFLAGS) -DCOMM=pipe
libclient_pipe_la_SOURCES = client_api_pipe.c $(BASE_SRCS)

noinst_LTLIBRARIES += libclient_socket.la
libclient_socket_la_CPPFLAGS = $(AM_CPPFLAGS) -DCOMM=socket
libclient_socket_la_SOURCES = client_api_socket.c $(BASE_SRCS)

noinst_LTLIBRARIES += libclient_biter.la
libclient_biter_la_CPPFLAGS = $(AM_CPPFLAGS) -DCOMM=biter -I$(top_srcdir)/../biter
libclient_biter_la_LIBADD = $(top_builddir)/biter/libbiterc.la
libclient_biter_la_SOURCES = client_api_biter.c $(BASE_SRCS)
