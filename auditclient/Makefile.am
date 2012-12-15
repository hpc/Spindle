lib_LTLIBRARIES = libldcs_audit_client_socket.la libldcs_audit_client_pipe.la libsampleA.la libsampleB.la
#noinst_LTLIBRARIES = 
#noinst_PROGRAMS = helloworld helloworld2 helloworld3 helloworld3_mpi

libldcs_audit_client_socket_la_SOURCES = ldcs_audit_client.c
libldcs_audit_client_socket_la_CPPFLAGS = -DBIT64 -I$(top_srcdir)/comlib
libldcs_audit_client_socket_la_LIBADD = $(top_builddir)/comlib/libldcs_socket.la

libldcs_audit_client_pipe_la_SOURCES = ldcs_audit_client.c
libldcs_audit_client_pipe_la_CPPFLAGS = -DBIT64 -I$(top_srcdir)/comlib
libldcs_audit_client_pipe_la_LIBADD = $(top_builddir)/comlib/libldcs_pipe.la

#libsampleA_la_SOURCES = libsampleA.c
#libsampleB_la_SOURCES = libsampleB.c
#libsampleA_la_CPPFLAGS = -DBIT64
#libsampleB_la_CPPFLAGS = -DBIT64

#helloworld_SOURCES = helloworld.c
#helloworld2_SOURCES = helloworld2.c
#helloworld2_LDADD = libsampleA.la libsampleB.la
#helloworld3_SOURCES = helloworld3.c
#helloworld3_LDADD = libsampleA.la -ldl
#helloworld3_mpi_SOURCES = helloworld3.c
#helloworld3_mpi_LDADD = $(MPI_CLDFLAGS) libsampleA.la -ldl
#helloworld3_mpi_CPPFLAGS = $(MPI_CFLAGS)

