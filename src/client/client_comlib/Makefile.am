noinst_LTLIBRARIES =

AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include

BASE_SRCS = client_api.c client_heap.c client_wrappers.c

noinst_LTLIBRARIES += libclient_pipe.la
libclient_pipe_la_CPPFLAGS = $(AM_CPPFLAGS) -DCOMM=pipe
libclient_pipe_la_SOURCES = client_api_pipe.c $(BASE_SRCS)

noinst_LTLIBRARIES += libclient_socket.la
libclient_socket_la_CPPFLAGS = $(AM_CPPFLAGS) -DCOMM=socket
libclient_socket_la_SOURCES = client_api_socket.c $(BASE_SRCS)

#noinst_LTLIBRARIES += libclient_shmem.la
#libclient_shmem_la_CPPFLAGS = $(AM_CPPFLAGS) -DCOMM=shmem
#libclient_shmem_la_SOURCES = client_api_shmem.c $(BASE_SRCS)
