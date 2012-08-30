DEBUG=NODEBUG
DEBUGD=-D$(DEBUG)
CFLAGS= -O3 -g $(DEBUGD) -I../lib
FFLAGS= -O3 -g
LDFLAGS = -g 

LIBS_SOCKET  = -g -L../lib -lldcs_socket
LIBS_PIPE    = -g -L../lib -lldcs_pipe

ifeq ($(DEBUG),SIONDEBUG)
 CFLAGS += -I../tools/sion_debug
 LIBS += -L../tools/sion_debug -lsiondebug
endif

all: default 

default: ldcs_simple_server_socket ldcs_simple_server_pipe

ldcs_simple_server_socket:   ldcs_simple_server.o
	 $(CC) $(CFLAGS) -o ldcs_simple_server_socket ldcs_simple_server.o $(LIBS_SOCKET) 

ldcs_simple_server_pipe:   ldcs_simple_server.o
	 $(CC) $(CFLAGS) -o ldcs_simple_server_pipe ldcs_simple_server.o $(LIBS_PIPE) 

ldcs_simple_server.o: 	ldcs_simple_server.c Makefile ../lib/libldcs_socket.a ../lib/libldcs_pipe.a
	$(CC) $(CFLAGS) -c  ldcs_simple_server.c	

clean:
	rm -rf *.o  *~ *core*
	rm -rf ldcs_simple_server_pipe ldcs_simple_server_socket

