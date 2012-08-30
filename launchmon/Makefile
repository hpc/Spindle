DEBUG=NODEBUG
DEBUGD=-D$(DEBUG)

# /collab/usr/global/tools/launchmon/build/chaos_5_x86_64_ib/launchmon-1.0.0-20120821
# export MANPATH=$MANPATH:/usr/global/tools/launchmon/chaos_5_x86_64_ib/launchmon-1.0.0-20120821/share/man

LAUNCHMON=/usr/global/tools/launchmon/chaos_5_x86_64_ib/launchmon-1.0.0-20120821

CFLAGS_LMON=-I$(LAUNCHMON)/include
LIBS_LMON_BE=-L$(LAUNCHMON)/lib -lmonbeapi
LIBS_LMON_FE=-L$(LAUNCHMON)/lib -lmonfeapi

CFLAGS= -O3 -g -Wall $(DEBUGD) -I. -I../lib -I../cache -I../auditserver 
FFLAGS= -O3 -g
LIBS_SOCKET_WO_WRAPPER  = -g -L../lib -lldcs_socket_wo_wrapper -L../cache -lldcs_cache
LIBS_SOCKET  = -g -L../lib -lldcs_socket -L../cache -lldcs_cache
LIBS_PIPE    = -g -L../lib -lldcs_pipe -L../cache -lldcs_cache

MD_CFLAGS=-DMD_ENABLED -I../auditserver  -DEXIT_AFTER_SESSION
MD_LDFLAGS=-DMD_ENABLED 

MSOCKET_CFLAGS=
MSOCKET_LDFLAGS= 

MPICC = mpicc

ifeq ($(DEBUG),SIONDEBUG)
 CFLAGS      += -I../tools/sion_debug
 LIBS_SOCKET_WO_WRAPPER += -L../tools/sion_debug -lsiondebug
 LIBS_SOCKET += -L../tools/sion_debug -lsiondebug
 LIBS_PIPE   += -L../tools/sion_debug -lsiondebug
endif

SERVER_PIPE_OBJS_MSOCKET=../auditserver/ldcs_audit_server_process.o ../auditserver/ldcs_audit_server_md_msocket.o \
	../auditserver/ldcs_audit_server_md_msocket_util.o  ../auditserver/ldcs_audit_server_md_msocket_topo.o \
        ../auditserver/ldcs_audit_server_client_cb.o ../auditserver/ldcs_audit_server_stateloop.o \
        ../auditserver/ldcs_audit_server_server_cb.o ../auditserver/ldcs_audit_server_filemngt.o


all: default 

default: sample_fe sample_be sample_client spindle_fe spindle_be 


# SPINDLE
#--------
spindle_external_fabric_fe.o: 	spindle_external_fabric_fe.c Makefile spindle_external_fabric.h
	$(CC) $(CFLAGS) $(MD_CFLAGS) $(CFLAGS_LMON) -c spindle_external_fabric_fe.c 	

spindle_fe.o: 	spindle_fe.c Makefile spindle_usrdata.h
	$(CC) $(CFLAGS) $(MD_CFLAGS) $(CFLAGS_LMON) -c spindle_fe.c 	

spindle_fe:  spindle_fe.o spindle_usrdata.o spindle_external_fabric_fe.o $(SERVER_PIPE_OBJS_MSOCKET)
	$(CC) $(CFLAGS) $(MD_LDFLAGS) -o spindle_fe spindle_fe.o spindle_usrdata.o spindle_external_fabric_fe.o \
	 $(SERVER_PIPE_OBJS_MSOCKET) $(LIBS_LMON_FE) $(LIBS_SOCKET)  

spindle_external_fabric_be.o: 	spindle_external_fabric_be.c Makefile spindle_external_fabric.h
	$(CC) $(CFLAGS) $(MD_CFLAGS) $(CFLAGS_LMON) -c spindle_external_fabric_be.c 	

spindle_be.o: 	spindle_be.c Makefile spindle_usrdata.h
	$(CC) $(CFLAGS) $(CFLAGS_LMON) -c spindle_be.c	

spindle_be:  spindle_be.o spindle_usrdata.o spindle_external_fabric_be.o $(SERVER_PIPE_OBJS_MSOCKET)
	$(CC) $(CFLAGS) $(MD_LDFLAGS) -o spindle_be spindle_be.o spindle_usrdata.o spindle_external_fabric_be.o \
	 $(SERVER_PIPE_OBJS_MSOCKET) $(LIBS_LMON_BE) $(LIBS_PIPE) $(LIBS_SOCKET_WO_WRAPPER)  


# Spindle
#-------
sample_client.o: 	sample_client.c Makefile
	$(MPICC) $(CFLAGS) -c  sample_client.c	

sample_fe.o: 	sample_fe.c Makefile sample_usrdata.h
	$(CC) $(CFLAGS) $(CFLAGS_LMON) -c sample_fe.c 	

sample_be.o: 	sample_be.c Makefile sample_usrdata.h
	$(CC) $(CFLAGS) $(CFLAGS_LMON) -c sample_be.c	

sample_usrdata.o: 	sample_usrdata.c Makefile
	$(CC) $(CFLAGS) $(CFLAGS_LMON) -c sample_usrdata.c	

sample_fe:  sample_fe.o sample_usrdata.o
	$(CC) $(CFLAGS) $(LIBS_LMON_FE) -o sample_fe sample_fe.o sample_usrdata.o

sample_be:  sample_be.o sample_usrdata.o
	$(CC) $(CFLAGS) $(LIBS_LMON_BE) -o sample_be sample_be.o sample_usrdata.o

sample_client:  sample_client.o 
	$(MPICC) $(CFLAGS) -o sample_client sample_client.o





clean::
	rm -rf *.o  *~ *core*
	rm -rf sample_be sample_fe sample_client

