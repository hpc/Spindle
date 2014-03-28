pkglibexec_LTLIBRARIES = libompiintercept.la
noinst_LTLIBRARIES = libparseompi.la

libompiintercept_la_SOURCES = ompi_intercept.c
libparseompi_la_SOURCES = parse_openmpi.cc

AM_CPPFLAGS = -I$(top_srcdir)/../include -I$(top_srcdir)/../logging -I$(top_srcdir)/startup
libparseompi_la_CPPFLAGS  = $(AM_CPPFLAGS) -DLIBEXECDIR=\"$(pkglibexecdir)\"
