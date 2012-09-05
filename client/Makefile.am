noinst_PROGRAMS = ldcs_simple_client_socket ldcs_simple_client_pipe ldcs_simple_client_pipe_mpi

AM_CPPFLAGS = -I$(top_srcdir)/comlib

ldcs_simple_client_socket_SOURCES = ldcs_simple_client.c
ldcs_simple_client_socket_LDADD = $(top_builddir)/comlib/libldcs_socket.la
ldcs_simple_client_pipe_SOURCES = ldcs_simple_client.c
ldcs_simple_client_pipe_LDADD = $(top_builddir)/comlib/libldcs_pipe.la
ldcs_simple_client_pipe_mpi_SOURCES =  ldcs_simple_client_mpi.c
ldcs_simple_client_pipe_mpi_LDADD = $(MPI_CLDFLAGS) $(top_builddir)/comlib/libldcs_pipe.la
ldcs_simple_client_pipe_mpi_CPPFLAGS = $(AM_CPPFLAGS) $(MPI_CFLAGS)



