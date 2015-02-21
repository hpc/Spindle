pkglibexec_LTLIBRARIES = libspindleint.la
noinst_LTLIBRARIES = libsubaudit.la

AM_CFLAGS = -fvisibility=hidden

AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -I$(top_srcdir)/auditclient -I$(top_srcdir)/client -I$(top_srcdir)/client_comlib

BASE_SRCS = subaudit.c lookup_libc.c intercept_malloc.c update_pltbind.c

libsubaudit_la_SOURCES = $(BASE_SRCS)

libspindleint_la_SOURCES = preloadlib.c
libspindleint_la_LDFLAGS = -shared -avoid-version
