#noinst_LTLIBRARIES = libaudit_server_msocket.la libaudit_server_cobo.la libserverbase.la
noinst_LTLIBRARIES = libaudit_server_cobo.la libserverbase.la

AM_CPPFLAGS = -I$(top_srcdir)/comlib -I$(top_srcdir)/cache -I$(top_srcdir)/../cobo -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -I$(top_srcdir)/../utils -DLIBEXECDIR=\"$(pkglibexecdir)\"
LDADD = $(top_builddir)/cache/libldcs_cache.la -lrt
#AM_LDFLAGS = -all-static

libserverbase_la_SOURCES = ldcs_audit_server_client_cb.c ldcs_audit_server_server_cb.c ldcs_audit_server_process.c ldcs_audit_server_filemngt.c ldcs_audit_server_handlers.c ldcs_elf_read.c ldcs_audit_server_requestors.c msgbundle.c parse_mounts.cc cleanup_proc.cc
libserverbase_la_LIBADD = -lpthread

#libaudit_server_msocket_la_SOURCES = ldcs_audit_server_md_msocket.c ldcs_audit_server_md_msocket_util.c ldcs_audit_server_md_msocket_topo.c 
libaudit_server_cobo_la_SOURCES = ldcs_audit_server_md_cobo.c

LCD = $(top_builddir)/comlib
COD = $(top_builddir)/cobo/
#libaudit_server_msocket_la_LIBADD = $(LDADD) libserverbase.la
libaudit_server_cobo_la_LIBADD = $(LDADD) libserverbase.la $(COD)/libldcs_cobo.la
