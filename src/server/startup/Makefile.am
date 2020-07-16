pkglibexec_PROGRAMS = spindle_be
lib_LTLIBRARIES = libspindlebe.la

AM_CPPFLAGS = -I$(top_srcdir)/../logging

CORE_SOURCES = spindle_be.cc hookexec.c $(top_srcdir)/../utils/parseloc.c $(top_srcdir)/../utils/keyfile.c $(top_srcdir)/../utils/spindle_mkdir.c $(top_srcdir)/../utils/exitnote.c
CORE_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/comlib -I$(top_srcdir)/auditserver -I$(top_srcdir)/../client/beboot -I$(top_srcdir)/../include -I$(top_srcdir)/../utils -I$(top_srcdir)/../cobo
CORE_LDADD = $(top_builddir)/logging/libspindledlogc.la -ldl
if COBO
CORE_LDADD += $(top_builddir)/auditserver/libaudit_server_cobo.la
endif
if MSOCKET
CORE_LDADD += $(top_builddir)/auditserver/libaudit_server_msocket.la
endif
if SOCKETS
CORE_LDADD += $(top_builddir)/comlib/libserver_socket.la
endif
if PIPES
CORE_LDADD += $(top_builddir)/comlib/libserver_pipe.la
endif
if BITER
CORE_LDADD += $(top_builddir)/comlib/libserver_biter.la $(top_builddir)/biter/libbiterd.la
endif
CORE_LDADD += $(GCRYPT_LIBS)

libspindlebe_la_CPPFLAGS = $(CORE_CPPFLAGS) -DSPINDLEBELIB
libspindlebe_la_SOURCES = $(CORE_SOURCES)
libspindlebe_la_LIBADD = $(CORE_LDADD) $(MUNGE_DYN_LIB)
libspindlebe_la_LDFLAGS = -version-info $(SPINDLEBE_LIB_VERSION)

spindle_be_LDFLAGS = -static
spindle_be_CPPFLAGS = $(CORE_CPPFLAGS)
spindle_be_SOURCES = spindle_be_main.cc spindle_be_serial.cc spindle_be_hostbin.cc spindle_be_mpilaunch.cc cleanup_proc.cc $(CORE_SOURCES)
spindle_be_LDADD = $(CORE_LDADD) $(MUNGE_LIBS)

if LINK_LIBSTDCXX_STATIC
spindle_be_LDFLAGS += $(STATIC_LIBGCC_OPT) -L.
CLEANFILES = ./libstdc++.a
./libstdc++.a:
	$(AM_V_GEN)$(LN_S) $(STATIC_LIBSTDCXX) $@
BUILT_SOURCES=./libstdc++.a
endif

if LMON
if LMON_DYNAMIC
spindle_be_LDADD += $(top_builddir)/launchmon/libbelmon.la $(LAUNCHMON_LIB) $(LAUNCHMON_RMCOMM) -lmonbeapi -lgcrypt -lpthread
else
spindle_be_LDADD += $(top_builddir)/launchmon/libbelmon.la $(LAUNCHMON_STATIC_LIBS)
endif
endif
