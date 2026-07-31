pkglibexec_PROGRAMS = spindle_bootstrap

spindle_bootstrap_LDFLAGS = $(AM_LDFLAGS) $(STATICFLAG)
spindle_bootstrap_CPPFLAGS = $(AM_CPPFLAGS) -DLIBEXECDIR=\"$(pkglibexecdir)\" -DPROGLIBDIR=\"$(pkglibdir)\" -I$(top_srcdir)/../include -I$(top_srcdir)/../logging -I$(top_srcdir)/client_comlib -I$(top_srcdir)/client -I$(top_srcdir)/shm_cache -I$(top_srcdir)/../utils
spindle_bootstrap_LDADD = $(top_builddir)/logging/libspindleclogc.la $(top_builddir)/shm_cache/libshmcache.la $(top_builddir)/client_comlib/libclient_pipe.la
spindle_bootstrap_SOURCES = spindle_bootstrap.c $(top_srcdir)/../utils/parseloc.c $(top_srcdir)/../utils/spindle_mkdir.c $(top_srcdir)/../utils/getcpu.c $(top_srcdir)/client/exec_util.c  $(top_srcdir)/client/lookup.c $(top_srcdir)/../utils/fileutil.c
