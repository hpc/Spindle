pkglibexec_LTLIBRARIES =

if PIPES
pkglibexec_LTLIBRARIES += libspindle_instr_pipe.la
endif

AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -I$(top_srcdir)/client -I$(top_srcdir)/client_comlib

BASE_SRCS = intercept.c

INSTRLIB = $(top_builddir)/client/libspindle_instr.la

AM_CFLAGS = -fvisibility=hidden

libspindle_instr_pipe_la_SOURCES = $(BASE_SRCS)
libspindle_instr_pipe_la_LIBADD = $(top_builddir)/client/libspindlec_pipe.la $(INSTRLIB)
libspindle_instr_pipe_la_LDFLAGS = -shared -avoid-version
