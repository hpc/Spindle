noinst_PROGRAMS = simulator_pipe_mpi_cobo simulator_pipe_mpi_msocket

CC = mpicc

LCD = $(top_builddir)/comlib
CAD = $(top_builddir)/cache
COD = $(top_builddir)/tools/cobo/src
AM_CPPFLAGS = -I$(top_srcdir)/comlib -I$(top_srcdir)/cache -I$(top_srcdir)/auditserver -I$(top_srcdir)/tools/cobo/src
LDADD = $(LCD)/libldcs_pipe.la $(CAD)/libldcs_cache.la $(COD)/libldcs_cobo.la $(COD)/libcobocomm.la

simulator_pipe_mpi_cobo_SOURCES = simulator_mpi.c simulator_client.c simulator_server.c simulator_fe.c $(top_srcdir)/auditserver/ldcs_audit_server_process.c $(top_srcdir)/auditserver/ldcs_audit_server_md_cobo.c $(top_srcdir)/auditserver/ldcs_audit_server_client_cb.c $(top_srcdir)/auditserver/ldcs_audit_server_stateloop.c $(top_srcdir)/auditserver/ldcs_audit_server_server_cb.c $(top_srcdir)/auditserver/ldcs_audit_server_filemngt.c $(top_srcdir)/auditserver/ldcs_elf_read.c

simulator_pipe_mpi_msocket_SOURCES = simulator_mpi.c simulator_client.c simulator_server.c simulator_fe.c $(top_srcdir)/auditserver/ldcs_audit_server_process.c $(top_srcdir)/auditserver/ldcs_audit_server_md_msocket.c $(top_srcdir)/auditserver/ldcs_audit_server_md_msocket_util.c  $(top_srcdir)/auditserver/ldcs_audit_server_md_msocket_topo.c $(top_srcdir)/auditserver/ldcs_audit_server_client_cb.c $(top_srcdir)/auditserver/ldcs_audit_server_stateloop.c $(top_srcdir)/auditserver/ldcs_audit_server_server_cb.c $(top_srcdir)/auditserver/ldcs_audit_server_filemngt.c $(top_srcdir)/auditserver/ldcs_elf_read.c
simulator_pipe_mpi_msocket_LDADD = $(LCD)/libldcs_socket_wo_wrapper.la $(LDADD)
