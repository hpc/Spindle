pkglibexec_PROGRAMS = spindle_bootstrap

spindle_bootstrap_LDFLAGS = -all-static $(AM_LDFLAGS)
spindle_bootstrap_CPPFLAGS = $(AM_CPPFLAGS) -DLIBEXECDIR=\"$(pkglibexecdir)\" -DPROGLIBDIR=\"$(pkglibdir)\" -I$(top_srcdir)/../include -I$(top_srcdir)/../logging -I$(top_srcdir)/client_comlib -I$(top_srcdir)/client -I$(top_srcdir)/shm_cache
spindle_bootstrap_LDADD = $(top_builddir)/logging/libspindleclogc.la $(top_builddir)/shm_cache/libshmcache.la
spindle_bootstrap_SOURCES = spindle_bootstrap.c $(top_srcdir)/../utils/parseloc.c $(top_srcdir)/../utils/spindle_mkdir.c $(top_srcdir)/client/exec_util.c 

if PIPES
spindle_bootstrap_LDADD += $(top_builddir)/client_comlib/libclient_pipe.la
endif
if BITER
spindle_bootstrap_LDADD += $(top_builddir)/client_comlib/libclient_biter.la $(top_builddir)/biter/libbiterc.la
endif
