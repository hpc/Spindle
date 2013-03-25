pkglibexec_PROGRAMS = spindle_bootstrap

spindle_bootstrap_LDFLAGS = -all-static $(AM_LDFLAGS)
spindle_bootstrap_CPPFLAGS = $(AM_CPPFLAGS) -DLIBDIR=\"$(libdir)\" -I$(top_srcdir)/comlib -I$(top_srcdir)/logging
spindle_bootstrap_LDADD = $(top_builddir)/comlib/libldcs_pipe.la $(top_builddir)/logging/libspindlelogc.la
spindle_bootstrap_SOURCES = spindle_bootstrap.c
