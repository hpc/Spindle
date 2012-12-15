lib_LTLIBRARIES = libaudit_server_msocket.la libaudit_server_cobo.la
noinst_LTLIBRARIES = libserverbase.la

#bin_PROGRAMS = ldcs_audit_server_socket ldcs_audit_server_pipe ldcs_audit_server_par_pipe ldcs_audit_server_par_pipe_md ldcs_audit_server_par_pipe_msocket

AM_CPPFLAGS = -I$(top_srcdir)/comlib -I$(top_srcdir)/cache -I$(top_srcdir)/tools/cobo/src
LDADD = $(top_builddir)/cache/libldcs_cache.la
AM_LDFLAGS = -all-static

#ldcs_audit_server_socket_SOURCES = ldcs_audit_server.c
#ldcs_audit_server_pipe_SOURCES = ldcs_audit_server.c

libserverbase_la_SOURCES = ldcs_audit_server_client_cb.c ldcs_audit_server_server_cb.c ldcs_audit_server_par.c ldcs_audit_server_process.c ldcs_audit_server_filemngt.c ldcs_audit_server_stateloop.c

libaudit_server_msocket_la_SOURCES = ldcs_audit_server_md_msocket.c ldcs_audit_server_md_msocket_util.c ldcs_audit_server_md_msocket_topo.c
libaudit_server_cobo_la_SOURCES = ldcs_audit_server_md_cobo.c

LCD = $(top_builddir)/comlib
COD = $(top_builddir)/tools/cobo/src
libaudit_server_msocket_la_LIBADD = $(LDADD) libserverbase.la
libaudit_server_cobo_la_LIBADD = $(LDADD) libserverbase.la $(COD)/libldcs_cobo.la $(COD)/libcobocomm.la

#ldcs_audit_server_par_pipe_SOURCES = ldcs_audit_server_md_none.c
#ldcs_audit_server_par_pipe_md_SOURCES = ldcs_audit_server_md_cobo.c
#ldcs_audit_server_par_pipe_msocket_SOURCES = ldcs_audit_server_md_msocket.c ldcs_audit_server_md_msocket_util.c ldcs_audit_server_md_msocket_topo.c


#ldcs_audit_server_socket_LDADD = $(LDADD) $(LCD)/libldcs_socket.la
#ldcs_audit_server_pipe_LDADD = $(LDADD) $(LCD)/libldcs_pipe.la
#ldcs_audit_server_par_pipe_LDADD = $(LDADD) $(LCD)/libldcs_pipe.la
#ldcs_audit_server_par_pipe_md_LDADD = $(LDADD) $(LCD)/libldcs_pipe.la $(COD)/libcobo.la $(COD)/libcobocomm.la
#ldcs_audit_server_par_pipe_msocket_LDADD = $(LDADD) $(LCD)/libldcs_pipe.la $(LCD)/libldcs_socket_wo_wrapper.la $(COD)/libcobo.la $(COD)/libcobocomm.la
