libver=`$(top_srcdir)/LIB_VERSION spindleflux`

pkglibexec_PROGRAMS = spindle_flux_session
lib_LTLIBRARIES = libspindleflux.la
noinst_LTLIBRARIES = libfluxsession.la

rcdir = $(PKGSYSCONF_DIR)
rc_DATA = spindle.rc

libspindleflux_la_SOURCES = \
	flux-spindle.c

libspindleflux_la_CPPFLAGS = \
	-I$(top_srcdir)/src/include \
	-I$(top_srcdir)/src/utils \
	$(FLUX_CORE_CFLAGS) \
	$(FLUX_HOSTLIST_CFLAGS)

libspindleflux_la_LIBADD = \
	$(top_builddir)/src/server/startup/libspindlebe_static.la \
	$(top_builddir)/src/fe/startup/libspindlefe_static.la \
	$(FLUX_CORE_LIBS) \
	$(FLUX_HOSTLIST_LIBS) \
	libfluxsession.la

libspindleflux_la_LDFLAGS = \
	$(fluxplugin_ldflags) \
   -Wl,--allow-multiple-definition \
   -version-info $(libver) \
	-module

spindle_flux_session_SOURCES = main.cc $(top_srcdir)/src/utils/spindle_mkdir.c $(top_srcdir)/src/utils/parseloc.c
spindle_flux_session_CPPFLAGS = -I$(top_srcdir)/src/flux -I$(top_srcdir)/src/logging -I$(top_srcdir)/src/include
spindle_flux_session_LDADD = libfluxsession.la

libfluxsession_la_SOURCES = sessionmgr.c fluxmgr.c procmgr.c spindlefemgr.c spindlebemgr.c
libfluxsession_la_CPPFLAGS = -I$(top_srcdir)/src/include -I$(top_srcdir)/src/flux $(FLUX_CORE_CFLAGS) $(FLUX_HOSTLIST_CFLAGS)
libfluxsession_la_LDFLAGS = $(fluxplugin_ldflags) -Wl,--allow-multiple-definition
libfluxsession_la_LIBADD = \
	$(top_builddir)/src/server/startup/libspindlebe_static.la \
	$(top_builddir)/src/fe/startup/libspindlefe_static.la \
	$(FLUX_CORE_LIBS) \
	$(FLUX_HOSTLIST_LIBS)

spindle.rc: $(srcdir)/spindle_rc $(top_builddir)/Makefile
	@rm -f ./spindle.rc
	$(AM_V_GEN)$(SED) -e s,SPINDLE_LIB,$(libdir),g < $(srcdir)/spindle_rc > $(top_builddir)/src/flux/spindle.rc

mostlyclean-local:
	rm -f ./spindle.rc
