lib_LTLIBRARIES = libspindleslurm.la

libver=`$(top_srcdir)/LIB_VERSION spindleslurm`

libspindleslurm_la_SOURCES = encode_decode.c plugin_utils.c slurm_plugin.c $(top_srcdir)/src/utils/spindle_mkdir.c $(top_srcdir)/src/utils/parseloc.c
libspindleslurm_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/src/include -I$(top_srcdir)/src/logging -DUSE_PLUGIN_DEBUG -DDEBUG -DCUSTOM_GETENV -DCUSTOM_GETENV_FREE -DSCONTROL_BIN="$(SCONTROL_ABSPATH)" -DSPINDLE_DO_EXPORT
libspindleslurm_la_LDFLAGS = $(AM_LDFLAGS) -ldl -version-info $(libver)
libspindleslurm_la_LIBADD = $(top_builddir)/src/server/startup/libspindlebe.la $(top_builddir)/src/fe/startup/libspindlefe.la
libspindleslurm_la_CFLAGS = $(CFLAGS) -fvisibility=hidden
libspindleslurm_la_CXXFLAGS = $(CXXFLAGS) -fvisibility=hidden
