pkglibexec_LTLIBRARIES = 

if SOCKETS
pkglibexec_LTLIBRARIES += libspindle_audit_socket.la
endif
if PIPES
pkglibexec_LTLIBRARIES += libspindle_audit_pipe.la
endif
if BITER
pkglibexec_LTLIBRARIES += libspindle_audit_biter.la
endif

AM_CFLAGS = -fvisibility=hidden

AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -I$(top_srcdir)/client -I$(top_srcdir)/client_comlib

BASE_SRCS = auditclient_common.c patch_linkmap.c redirect.c

AUDITLIBS = $(top_builddir)/client/libspindle_audit.la $(top_builddir)/subaudit/libsubaudit.la $(top_builddir)/auditv1/libauditv1.la

libspindle_audit_socket_la_SOURCES = $(BASE_SRCS)
libspindle_audit_socket_la_LIBADD = $(top_builddir)/client/libspindlec_socket.la $(AUDITLIBS)
libspindle_audit_socket_la_LDFLAGS = -shared -avoid-version

libspindle_audit_pipe_la_SOURCES = $(BASE_SRCS)
libspindle_audit_pipe_la_LIBADD = $(top_builddir)/client/libspindlec_pipe.la $(AUDITLIBS)
libspindle_audit_pipe_la_LDFLAGS = -shared -avoid-version

libspindle_audit_biter_la_SOURCES = $(BASE_SRCS)
libspindle_audit_biter_la_LIBADD = $(top_builddir)/client/libspindlec_biter.la $(AUDITLIBS)
libspindle_audit_biter_la_LDFLAGS = -shared -avoid-version
