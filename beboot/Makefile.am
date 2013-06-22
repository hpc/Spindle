pkglibexec_PROGRAMS = spindle_bootstrap

spindle_bootstrap_LDFLAGS = -all-static $(AM_LDFLAGS)
spindle_bootstrap_CPPFLAGS = $(AM_CPPFLAGS) -DLIBEXECDIR=\"$(pkglibexecdir)\" -I$(top_srcdir)/comlib -I$(top_srcdir)/logging -I$(top_srcdir)/auditclient -I$(top_srcdir)/client_comlib -I$(top_srcdir)/launchmon
spindle_bootstrap_LDADD = $(top_builddir)/logging/libspindlelogc.la $(top_builddir)/client_comlib/libclient_pipe.la
spindle_bootstrap_SOURCES = spindle_bootstrap.c parseloc.c $(top_srcdir)/auditclient/exec_util.c
