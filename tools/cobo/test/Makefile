DEBUG=NODEBUG
DEBUGD=-D$(DEBUG)
CFLAGS= -O3 -g $(DEBUGD) -I../src
FFLAGS= -O3 -g
LDFLAGS = -g 

LDCS_CFLAGS= -O3 -g $(DEBUGD) -I../../../auditserver -I../../../lib
LDCS_LIBS  = -g -L../../lib

SERVER_PIPE_OBJS_COBO=../../../auditserver/ldcs_audit_server_process.o \
	../../../auditserver/ldcs_audit_server_md_cobo.o \
	../../../auditserver/ldcs_audit_server_filemngt.o \
	../../../auditserver/ldcs_audit_server_client_cb.o \
	../../../auditserver/ldcs_audit_server_stateloop.o \
        ../../../auditserver/ldcs_audit_server_server_cb.o

MD_CFLAGS=-DMD_ENABLED -I..//src -DEXIT_AFTER_SESSION
MD_LDFLAGS=-DMD_ENABLED -L..//src -lcobo  -lcobocomm

LIBS  = -g -L../src -lcobo
LIBS_PIPE    = -g -L../../../lib -lldcs_pipe -L../../../cache -lldcs_cache

ifeq ($(DEBUG),SIONDEBUG)
 CFLAGS += -I../../../tools/sion_debug
 LIBS += -L../../../tools/sion_debug -lsiondebug
 LDCS_CFLAGS += -I../../../tools/sion_debug
 LDCS_LIBS += -L../../../tools/sion_debug -lsiondebug
endif

all: default 

default: client server_rsh server_rsh_ldcs server_rsh_ldcs_preload

client:   client.o
	 $(CC) $(CFLAGS) -o client client.o $(LIBS) 

client.o: client.c Makefile ../src/libcobo.a
	$(CC) $(CFLAGS) -c  client.c	

server_rsh: server_rsh.o
	 $(CC) $(CFLAGS) -o server_rsh server_rsh.o $(LIBS) 

server_rsh.o: server_rsh.c Makefile ../src/libcobo.a
	$(CC) $(CFLAGS) -c server_rsh.c	

server_rsh_ldcs: server_rsh_ldcs.o
	 $(CC) $(CFLAGS) -o server_rsh_ldcs server_rsh_ldcs.o $(LIBS) -lcobocomm

server_rsh_ldcs.o: server_rsh_ldcs.c Makefile ../src/libcobo.a ../src/libcobocomm.a
	$(CC) $(CFLAGS) -c server_rsh_ldcs.c	

server_rsh_ldcs_preload: server_rsh_ldcs_preload.o $(SERVER_PIPE_OBJS_COBO)
	 $(CC) $(CFLAGS) -o server_rsh_ldcs_preload server_rsh_ldcs_preload.o -lcobocomm $(SERVER_PIPE_OBJS_COBO) $(LIBS_PIPE) $(MD_LDFLAGS) $(LDCS_LIBS) 

server_rsh_ldcs_preload.o: server_rsh_ldcs_preload.c Makefile ../src/libcobo.a ../src/libcobocomm.a
	$(CC) $(LDCS_CFLAGS) -c server_rsh_ldcs_preload.c	

clean:
	rm -rf *.o  *~ *core*
	rm -rf client server_rsh server_rsh_ldcs

