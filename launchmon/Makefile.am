bin_PROGRAMS = spindle
pkglibexec_PROGRAMS = spindle_be

AM_LDFLAGS = $(LAUNCHMON_LIB)
AM_CPPFLAGS = $(LAUNCHMON_INC) -I$(top_srcdir)/logging

COD = $(top_builddir)/tools/cobo/src

spindle_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/comlib -I$(top_srcdir)/cache -I$(top_srcdir)/auditserver -DBINDIR=\"$(pkglibexecdir)\"
spindle_SOURCES = spindle_fe_main.cc spindle_fe_lmon.cc spindle_fe_serial.cc spindle_fe.cc parse_launcher.cc parseargs.cc parse_preload.cc

spindle_be_LDFLAGS = $(AM_LDFLAGS) -static
spindle_be_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/comlib -I$(top_srcdir)/auditserver -I$(top_srcdir)/beboot
spindle_be_SOURCES = spindle_be_main.cc spindle_be_lmon.cc spindle_be_serial.cc spindle_be.cc $(top_srcdir)/beboot/parseloc.c
spindle_be_LDADD = $(top_builddir)/cache/libldcs_cache.la $(top_builddir)/logging/libspindlelogc.la

if LINK_LIBSTDCXX_STATIC
spindle_be_LDFLAGS += $(STATIC_LIBGCC_OPT) -L.
CLEANFILES = ./libstdc++.a
./libstdc++.a:
	$(AM_V_GEN)$(LN_S) $(STATIC_LIBSTDCXX) $@
BUILT_SOURCES=./libstdc++.a
endif

if LMON_DYNAMIC
spindle_be_LDADD += -lmonbeapi -lcobo -lgcrypt
else
spindle_be_LDADD += $(LAUNCHMON_STATIC_LIBS)
endif

spindle_LDADD = -lmonfeapi $(top_builddir)/logging/libspindlelogc.la
if COBO
spindle_LDADD += $(top_builddir)/auditserver/libaudit_server_cobo.la
spindle_be_LDADD += $(top_builddir)/auditserver/libaudit_server_cobo.la
endif
if MSOCKET
spindle_LDADD += $(top_builddir)/auditserver/libaudit_server_msocket.la
spindle_be_LDADD += $(top_builddir)/auditserver/libaudit_server_msocket.la
endif
if SOCKETS
spindle_LDADD += $(top_builddir)/comlib/libserver_socket.la
spindle_be_LDADD += $(top_builddir)/comlib/libserver_socket.la
endif
if PIPES
spindle_LDADD += $(top_builddir)/comlib/libserver_pipe.la
spindle_be_LDADD += $(top_builddir)/comlib/libserver_pipe.la
endif
spindle_LDADD += -lmonfeapi $(top_builddir)/cache/libldcs_cache.la 
