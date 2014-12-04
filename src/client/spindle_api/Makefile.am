lib_LTLIBRARIES = libspindle.la
include_HEADERS = spindle.h

AM_CFLAGS = -fvisibility=hidden

AM_CPPFLAGS = -I$(top_builddir) -DSPINDLE_INTERNAL_BUILD
libspindle_la_LDFLAGS = -shared -version-info $(LIBSPINDLE_LIB_VERSION)
libspindle_la_SOURCES = spindle_api.c
