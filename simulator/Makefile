DEBUG=NODEBUG
DEBUGD=-D$(DEBUG)
CFLAGS= -O3 -g -Wall $(DEBUGD) -I../lib -I../cache -I../auditserver 
FFLAGS= -O3 -g
LIBS_SOCKET_WO_WRAPPER  = -g -L../lib -lldcs_socket_wo_wrapper -L../cache -lldcs_cache
LIBS_SOCKET  = -g -L../lib -lldcs_socket -L../cache -lldcs_cache
LIBS_PIPE    = -g -L../lib -lldcs_pipe -L../cache -lldcs_cache

MD_CFLAGS=-DMD_ENABLED -I../tools/cobo/src -DEXIT_AFTER_SESSION
MD_LDFLAGS=-DMD_ENABLED -L../tools/cobo/src -lcobo  -lcobocomm

MSOCKET_CFLAGS=-DMD_ENABLED -DEXIT_AFTER_SESSION
MSOCKET_LDFLAGS=-DMD_ENABLED 

MPICC = mpicc

ifeq ($(DEBUG),SIONDEBUG)
 CFLAGS      += -I../tools/sion_debug
 LIBS_SOCKET_WO_WRAPPER += -L../tools/sion_debug -lsiondebug
 LIBS_SOCKET += -L../tools/sion_debug -lsiondebug
 LIBS_PIPE   += -L../tools/sion_debug -lsiondebug
endif

SERVER_PIPE_OBJS_COBO=../auditserver/ldcs_audit_server_process.o ../auditserver/ldcs_audit_server_md_cobo.o \
        ../auditserver/ldcs_audit_server_client_cb.o ../auditserver/ldcs_audit_server_stateloop.o \
        ../auditserver/ldcs_audit_server_server_cb.o ../auditserver/ldcs_audit_server_filemngt.o

SERVER_PIPE_OBJS_MSOCKET=../auditserver/ldcs_audit_server_process.o ../auditserver/ldcs_audit_server_md_msocket.o \
	../auditserver/ldcs_audit_server_md_msocket_util.o  ../auditserver/ldcs_audit_server_md_msocket_topo.o \
        ../auditserver/ldcs_audit_server_client_cb.o ../auditserver/ldcs_audit_server_stateloop.o \
        ../auditserver/ldcs_audit_server_server_cb.o ../auditserver/ldcs_audit_server_filemngt.o

all: default 

default: simulator_pipe_mpi_cobo simulator_pipe_mpi_msocket

simulator_client.o: 	simulator_client.c Makefile
	$(MPICC) $(CFLAGS) -c  simulator_client.c	

simulator_fe.o: 	simulator_fe.c Makefile
	$(MPICC) $(CFLAGS) $(MD_CFLAGS) -c  simulator_fe.c	

simulator_server.o: 	simulator_server.c Makefile $(SERVER_PIPE_OBJS_COBO)
	$(MPICC) $(CFLAGS) -c  simulator_server.c	

simulator_pipe_mpi_cobo:   simulator_mpi.o simulator_client.o simulator_server.o simulator_fe.o $(SERVER_PIPE_OBJS_COBO)
	 $(MPICC) $(CFLAGS) -o simulator_pipe_mpi_cobo simulator_mpi.o simulator_client.o simulator_server.o simulator_fe.o $(SERVER_PIPE_OBJS_COBO) $(LIBS_PIPE) $(MD_LDFLAGS)


simulator_pipe_mpi_msocket:   simulator_mpi.o simulator_client.o simulator_server.o simulator_fe.o $(SERVER_PIPE_OBJS_MSOCKET)
	 $(MPICC) $(CFLAGS) -o simulator_pipe_mpi_msocket simulator_mpi.o simulator_client.o simulator_server.o simulator_fe.o $(SERVER_PIPE_OBJS_MSOCKET) $(LIBS_PIPE) $(LIBS_SOCKET_WO_WRAPPER) $(MD_LDFLAGS)


simulator_mpi.o: simulator_mpi.c Makefile  ../lib/libldcs_socket.a ../lib/libldcs_pipe.a
	$(MPICC) $(CFLAGS) -c  simulator_mpi.c	

clean::
	rm -rf *.o  *~ *core*
	rm -rf simulator simulator_pipe simulator_pipe_mpi_cobo simulator_pipe_mpi_msocket simulator_socket

