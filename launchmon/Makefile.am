noinst_PROGRAMS = sample_fe sample_be sample_client
bin_PROGRAMS = spindle_fe spindle_be
AM_LDFLAGS = $(LAUNCHMON_LIB)
AM_CPPFLAGS = $(LAUNCHMON_INC)


sample_fe_SOURCES = sample_fe.c spindle_usrdata.c
sample_fe_LDADD = -lmonfeapi
sample_be_SOURCES = sample_be.c spindle_usrdata.c
sample_be_LDADD = -lmonbeapi
sample_client_SOURCES = sample_client.c
sample_client_LDADD = $(MPI_CLDFLAGS)
sample_client_CFLAGS = $(AM_CFLAGS) $(MPI_CFLAGS)

spindle_fe_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/comlib -I$(top_srcdir)/cache -I$(top_srcdir)/auditserver
spindle_fe_LDADD = -lmonfeapi $(top_builddir)/comlib/libldcs_socket.la $(top_builddir)/cache/libldcs_cache.la
spindle_fe_SOURCES = spindle_fe.c spindle_usrdata.c spindle_external_fabric_fe.c parse_launcher.c ../auditserver/ldcs_audit_server_process.c ../auditserver/ldcs_audit_server_md_msocket.c ../auditserver/ldcs_audit_server_md_msocket_util.c ../auditserver/ldcs_audit_server_md_msocket_topo.c ../auditserver/ldcs_audit_server_client_cb.c ../auditserver/ldcs_audit_server_stateloop.c ../auditserver/ldcs_audit_server_server_cb.c ../auditserver/ldcs_audit_server_filemngt.c

spindle_be_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/comlib -I$(top_srcdir)/cache -I$(top_srcdir)/auditserver
spindle_be_LDADD = -lmonbeapi $(top_builddir)/comlib/libldcs_socket.la $(top_builddir)/cache/libldcs_cache.la
spindle_be_SOURCES = spindle_be.c spindle_usrdata.c spindle_external_fabric_be.c parse_launcher.c ../auditserver/ldcs_audit_server_process.c ../auditserver/ldcs_audit_server_md_msocket.c ../auditserver/ldcs_audit_server_md_msocket_util.c ../auditserver/ldcs_audit_server_md_msocket_topo.c ../auditserver/ldcs_audit_server_client_cb.c ../auditserver/ldcs_audit_server_stateloop.c ../auditserver/ldcs_audit_server_server_cb.c ../auditserver/ldcs_audit_server_filemngt.c
 
