pkglib_LTLIBRARIES = libspindleint.la

if SOCKETS
pkglib_LTLIBRARIES += libspindle_subaudit_socket.la
endif
if PIPES
pkglib_LTLIBRARIES += libspindle_subaudit_pipe.la
endif
if BITER
pkglib_LTLIBRARIES += libspindle_subaudit_biter.la
endif

AM_CFLAGS = -fvisibility=hidden

AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -I$(top_srcdir)/client -I$(top_srcdir)/client_comlib -I$(top_srcdir)/auditclient -I$(top_srcdir)/../utils

BASE_SRCS = subaudit.c intercept_malloc.c update_pltbind.c ../auditclient/auditclient_common.c ../auditclient/patch_linkmap.c

AUDITLIB = $(top_builddir)/client/libspindle_audit.la

libspindleint_la_SOURCES = preloadlib.c
libspindleint_la_LDFLAGS = -shared -avoid-version

libspindle_subaudit_socket_la_SOURCES = $(BASE_SRCS)
libspindle_subaudit_socket_la_LIBADD = $(top_builddir)/client/libspindlec_socket.la $(AUDITLIB)
libspindle_subaudit_socket_la_LDFLAGS = -shared -avoid-version

libspindle_subaudit_pipe_la_SOURCES = $(BASE_SRCS)
libspindle_subaudit_pipe_la_LIBADD = $(top_builddir)/client/libspindlec_pipe.la $(AUDITLIB)
libspindle_subaudit_pipe_la_LDFLAGS = -shared -avoid-version

libspindle_subaudit_biter_la_SOURCES = $(BASE_SRCS)
libspindle_subaudit_biter_la_LIBADD = $(top_builddir)/client/libspindlec_biter.la $(AUDITLIB)
libspindle_subaudit_biter_la_LDFLAGS = -shared -avoid-version
