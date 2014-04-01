pkglibexec_PROGRAMS = spindle_be

AM_CPPFLAGS = -I$(top_srcdir)/../logging

spindle_be_LDFLAGS = -static
spindle_be_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/comlib -I$(top_srcdir)/auditserver -I$(top_srcdir)/../client/beboot -I$(top_srcdir)/../include -I$(top_srcdir)/../utils -I$(top_srcdir)/../cobo
spindle_be_SOURCES = spindle_be_main.cc spindle_be_serial.cc spindle_be_hostbin.cc spindle_be.cc $(top_srcdir)/../utils/parseloc.c $(top_srcdir)/../utils/keyfile.c
spindle_be_LDADD = $(top_builddir)/cache/libldcs_cache.la $(top_builddir)/logging/libspindlelogc.la
spindle_be_LDADD += $(MUNGE_LIBS) $(GCRYPT_LIBS)

if LINK_LIBSTDCXX_STATIC
spindle_be_LDFLAGS += $(STATIC_LIBGCC_OPT) -L.
CLEANFILES = ./libstdc++.a
./libstdc++.a:
	$(AM_V_GEN)$(LN_S) $(STATIC_LIBSTDCXX) $@
BUILT_SOURCES=./libstdc++.a
endif

if LMON
if LMON_DYNAMIC
spindle_be_LDADD += $(top_builddir)/launchmon/libbelmon.la $(LAUNCHMON_LIB) $(LAUNCHMON_RMCOMM) -lmonbeapi -lgcrypt
else
spindle_be_LDADD += $(LAUNCHMON_STATIC_LIBS)
endif
endif

if COBO
spindle_be_LDADD += $(top_builddir)/auditserver/libaudit_server_cobo.la
endif
if MSOCKET
spindle_be_LDADD += $(top_builddir)/auditserver/libaudit_server_msocket.la
endif
if SOCKETS
spindle_be_LDADD += $(top_builddir)/comlib/libserver_socket.la
endif
if PIPES
spindle_be_LDADD += $(top_builddir)/comlib/libserver_pipe.la
endif
if BITER
spindle_be_LDADD += $(top_builddir)/comlib/libserver_biter.la $(top_builddir)/biter/libbiterd.la
endif
