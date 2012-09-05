noinst_PROGRAMS = client server_rsh server_rsh_ldcs server_rsh_ldcs_preload

LSRD = $(top_srcdir)/auditserver
COD = $(top_builddir)/tools/cobo/src
CAD = $(top_builddir)/cache
LCD = $(top_builddir)/comlib

LDADD = $(COD)/libcobo.la
AM_CPPFLAGS = -I$(top_srcdir)/tools/cobo/src -I$(top_srcdir)/auditserver -I$(top_srcdir)/comlib -I$(top_srcdir)/cache

client_SOURCES = client.c
server_rsh_SOURCES = server_rsh.c
server_rsh_ldcs_SOURCES = server_rsh_ldcs.c
server_rsh_ldcs_preload_SOURCES = server_rsh_ldcs_preload.c $(LSRD)/ldcs_audit_server_process.c $(LSRD)/ldcs_audit_server_md_cobo.c $(LSRD)/ldcs_audit_server_filemngt.c $(LSRD)/ldcs_audit_server_client_cb.c $(LSRD)/ldcs_audit_server_stateloop.c $(LSRD)/ldcs_audit_server_server_cb.c

server_rsh_ldcs_LDADD = $(COD)/libcobocomm.la $(LCD)/libldcs_pipe.la $(CAD)/libldcs_cache.la $(LDADD)
server_rsh_ldcs_preload_LDADD = $(COD)/libcobocomm.la $(LCD)/libldcs_pipe.la $(CAD)/libldcs_cache.la $(LDADD)
