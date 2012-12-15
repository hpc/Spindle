bin_PROGRAMS = spindle_fe spindle_be

AM_LDFLAGS = $(LAUNCHMON_LIB)
AM_CPPFLAGS = $(LAUNCHMON_INC)

COD = $(top_builddir)/tools/cobo/src

spindle_fe_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/comlib -I$(top_srcdir)/cache -I$(top_srcdir)/auditserver -DBINDIR=\"$(bindir)\"
spindle_fe_SOURCES = spindle_fe.cc spindle_usrdata.cc spindle_external_fabric_fe.cc parse_launcher.cc

spindle_be_LDFLAGS = $(AM_LDFLAGS) -all-static
spindle_be_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/comlib -I$(top_srcdir)/auditserver
spindle_be_SOURCES = spindle_be.cc spindle_usrdata.cc spindle_external_fabric_be.cc
spindle_be_LDADD = -lmonbeapi $(top_builddir)/cache/libldcs_cache.la

spindle_fe_LDADD = -lmonfeapi
if COBO
spindle_fe_LDADD += $(top_builddir)/auditserver/libaudit_server_cobo.la
spindle_be_LDADD += $(top_builddir)/auditserver/libaudit_server_cobo.la
endif
if MSOCKET
spindle_fe_LDADD += $(top_builddir)/auditserver/libaudit_server_msocket.la
spindle_be_LDADD += $(top_builddir)/auditserver/libaudit_server_msocket.la
endif
if SOCKETS
spindle_fe_LDADD += $(top_builddir)/comlib/libldcs_socket.la
spindle_be_LDADD += $(top_builddir)/comlib/libldcs_socket.la
endif
if PIPES
spindle_fe_LDADD += $(top_builddir)/comlib/libldcs_pipe.la
spindle_be_LDADD += $(top_builddir)/comlib/libldcs_pipe.la
endif
spindle_fe_LDADD += -lmonfeapi $(top_builddir)/cache/libldcs_cache.la 
