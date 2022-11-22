shell_plugin_LTLIBRARIES = \
	spindle.la

spindle_la_SOURCES = \
	flux-spindle.c

spindle_la_CPPFLAGS = \
	-I$(top_srcdir)/src/include \
	$(FLUX_CORE_CFLAGS) \
	$(FLUX_HOSTLIST_CFLAGS)

spindle_la_LIBADD = \
	$(FLUX_CORE_LIBS) \
	$(FLUX_HOSTLIST_LIBS) \
	$(top_builddir)/src/server/startup/libspindlebe.la \
	$(top_builddir)/src/fe/startup/libspindlefe.la

spindle_la_LDFLAGS = \
	$(fluxplugin_ldflags) \
	-module
