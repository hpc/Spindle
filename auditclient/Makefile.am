lib_LTLIBRARIES = 
noinst_LTLIBRARIES = 

if SOCKETS
lib_LTLIBRARIES += libspindle_client_socket.la
else
noinst_LTLIBRARIES += libspindle_client_socket.la
endif
if PIPES
lib_LTLIBRARIES += libspindle_client_pipe.la
else
noinst_LTLIBRARIES += libspindle_client_pipe.la
endif

AM_CPPFLAGS = -I$(top_srcdir)/logging

libspindle_client_socket_la_SOURCES = ldcs_audit_client.c
libspindle_client_socket_la_CPPFLAGS = $(AM_CPPFLAGS) -DBIT64 -I$(top_srcdir)/comlib
libspindle_client_socket_la_LIBADD = $(top_builddir)/comlib/libldcs_socket.la $(top_builddir)/logging/libspindlelogc.la
libspindle_client_socket_la_LDFLAGS = -shared

libspindle_client_pipe_la_SOURCES = ldcs_audit_client.c
libspindle_client_pipe_la_CPPFLAGS =$(AM_CPPFLAGS)  -DBIT64 -I$(top_srcdir)/comlib
libspindle_client_pipe_la_LIBADD = $(top_builddir)/comlib/libldcs_pipe.la $(top_builddir)/logging/libspindlelogc.la
libspindle_client_pipe_la_LDFLAGS = -shared
