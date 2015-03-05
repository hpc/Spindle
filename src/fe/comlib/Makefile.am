noinst_LTLIBRARIES = libfe_cobo.la
if SPAWNNET
noinst_LTLIBRARIES += libfe_spawnnet.la
endif
#noinst_LTLIBRARIES = libfe_msocket.la
AM_CPPFLAGS = -I$(top_srcdir)/../logging -I$(top_srcdir)/../include -I$(top_srcdir)/../cobo
libfe_cobo_la_SOURCES = cobo_fe_comm.c
if SPAWNNET
libfe_spawnnet_la_SOURCES = spawnnet_fe_comm.c -lspawn
endif

#libfe_msocket_la_SOURCES = msocket_fe_comm.c
