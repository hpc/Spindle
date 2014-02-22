noinst_LTLIBRARIES =

AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -I$(top_srcdir)/auditserver

BASE_SRCS = ldcs_api_util.c ldcs_api_listen.c ldcs_api_wrapper.c

noinst_LTLIBRARIES += libserver_pipe.la
libserver_pipe_la_CPPFLAGS = $(AM_CPPFLAGS) -Dcomm=pipe
libserver_pipe_la_SOURCES = ldcs_api_pipe.c ldcs_api_pipe_notify.c $(BASE_SRCS)

noinst_LTLIBRARIES += libserver_socket.la
libserver_socket_la_CPPFLAGS = $(AM_CPPFLAGS) -Dcomm=socket
libserver_socket_la_SOURCES = ldcs_api_socket.c $(BASE_SRCS)

noinst_LTLIBRARIES += libserver_biter.la
libserver_biter_la_CPPFLAGS = $(AM_CPPFLAGS) -Dcomm=biter -I$(top_srcdir)/../biter
libserver_biter_la_SOURCES = ldcs_api_biter.c $(BASE_SRCS)

if SHMEM
noinst_LTLIBRARIES += libserver_shmem.la 
libserver_shmem_la_CPPFLAGS = $(AM_CPPFLAGS) -Dcomm=shmem
libserver_shmem_la_SOURCES = ldcs_api_shmem.c $(BASE_SRCS)
endif
