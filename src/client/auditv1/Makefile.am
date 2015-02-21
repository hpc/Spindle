noinst_LTLIBRARIES = libauditv1.la

AM_CFLAGS = -fvisibility=hidden

AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -I$(top_srcdir)/auditclient -I$(top_srcdir)/client

BASE_SRCS = auditclient.c
if X86_64_BLD
ARCH_SRCS = auditclient_x86_64.c
endif
if PPC64_BLD
ARCH_SRCS = auditclient_ppc64.c
endif

libauditv1_la_SOURCES = $(BASE_SRCS) $(ARCH_SRCS)
