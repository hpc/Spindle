pkglibexec_LTLIBRARIES = 

if SOCKETS
pkglibexec_LTLIBRARIES += libspindle_instr_socket.la
endif
if PIPES
pkglibexec_LTLIBRARIES += libspindle_instr_pipe.la
endif
if BITER
pkglibexec_LTLIBRARIES += libspindle_instr_biter.la
endif

AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -I$(top_srcdir)/client -I$(top_srcdir)/client_comlib

BASE_SRCS = intercept.c

INSTRLIB = $(top_builddir)/client/libspindle_instr.la

AM_CFLAGS = -fvisibility=hidden

libspindle_instr_socket_la_SOURCES = $(BASE_SRCS)
libspindle_instr_socket_la_LIBADD = $(top_builddir)/client/libspindlec_socket.la $(INSTRLIB)
libspindle_instr_socket_la_LDFLAGS = -shared -avoid-version

libspindle_instr_pipe_la_SOURCES = $(BASE_SRCS)
libspindle_instr_pipe_la_LIBADD = $(top_builddir)/client/libspindlec_pipe.la $(INSTRLIB)
libspindle_instr_pipe_la_LDFLAGS = -shared -avoid-version

libspindle_instr_biter_la_SOURCES = $(BASE_SRCS)
libspindle_instr_biter_la_LIBADD = $(top_builddir)/client/libspindlec_biter.la $(INSTRLIB)
libspindle_instr_biter_la_LDFLAGS = -shared -avoid-version
