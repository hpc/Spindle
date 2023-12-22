libver=`$(top_srcdir)/LIB_VERSION spindleflux`

lib_LTLIBRARIES = \
	libspindleflux.la

libspindleflux_la_SOURCES = \
	flux-spindle.c

libspindleflux_la_CPPFLAGS = \
	-I$(top_srcdir)/src/include \
	$(FLUX_CORE_CFLAGS) \
	$(FLUX_HOSTLIST_CFLAGS)

libspindleflux_la_LIBADD = \
	$(FLUX_CORE_LIBS) \
	$(FLUX_HOSTLIST_LIBS) \
	$(top_builddir)/src/server/startup/libspindlebe.la \
	$(top_builddir)/src/fe/startup/libspindlefe.la

libspindleflux_la_LDFLAGS = \
	$(fluxplugin_ldflags) \
   -version-info $(libver) \
	-module
