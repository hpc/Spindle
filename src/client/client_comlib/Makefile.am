noinst_LTLIBRARIES =

AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include

BASE_SRCS = client_api.c client_heap.c client_wrappers.c

AM_CFLAGS = -fvisibility=hidden

noinst_LTLIBRARIES += libclient_pipe.la
libclient_pipe_la_CPPFLAGS = $(AM_CPPFLAGS) -DCOMM=pipe
libclient_pipe_la_SOURCES = client_api_pipe.c $(BASE_SRCS)
