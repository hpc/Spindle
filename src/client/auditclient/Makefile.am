pkglib_LTLIBRARIES = libspindle_audit_pipe.la

AM_CFLAGS = -fvisibility=hidden

AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -I$(top_srcdir)/client -I$(top_srcdir)/client_comlib -I$(top_srcdir)/crash_handler

BASE_SRCS = auditclient.c auditclient_common.c patch_linkmap.c redirect.c bindgot.c writablegot.c patch_bad_dtv.c malloc_wrapper.c fixlocale.c
if X86_64_BLD
ARCH_SRCS = auditclient_x86_64.c 
endif
if PPC64_BLD
ARCH_SRCS = auditclient_ppc64.c
endif
if PPC64LE_BLD
ARCH_SRCS = auditclient_ppc64.c
endif
if AARCH64_BLD
ARCH_SRCS = auditclient_aarch64.c
endif

AUDITLIB = $(top_builddir)/client/libspindle_audit.la

libspindle_audit_pipe_la_SOURCES = $(BASE_SRCS) $(ARCH_SRCS)
libspindle_audit_pipe_la_LIBADD = $(top_builddir)/client/libspindlec_pipe.la $(AUDITLIB)
libspindle_audit_pipe_la_LDFLAGS = -shared -avoid-version
