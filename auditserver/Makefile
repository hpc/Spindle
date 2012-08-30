OPT=
DEBUG=NODEBUG
DEBUGD=-D$(DEBUG)

CFLAGS= -g $(OPT) $(DEBUGD) -I. -I../lib -I../cache -Wall
FFLAGS= -g $(OPT)
LDFLAGS = -g $(OPT) -L../cache -lldcs_cache


MD_CFLAGS_COBO=-DMD_ENABLED -I../tools/cobo/src -DEXIT_AFTER_SESSION
MD_LDFLAGS_COBO=-DMD_ENABLED -L../tools/cobo/src -lcobo  -lcobocomm

MD_CFLAGS_MSOCKET=-DMD_ENABLED -I../tools/cobo/src -DEXIT_AFTER_SESSION
MD_LDFLAGS_MSOCKET=-DMD_ENABLED -L../tools/cobo/src -lcobo  -lcobocomm

LIBS_SOCKET  = -g -L../lib -lldcs_socket
LIBS_SOCKET_WO_WRAPPER  = -g -L../lib -lldcs_socket_wo_wrapper
LIBS_PIPE    = -g -L../lib -lldcs_pipe

ifeq ($(DEBUG),SIONDEBUG)
 CFLAGS += -I../tools/sion_debug
 MD_CFLAGS += -I../tools/sion_debug
 LIBS_PIPE += -L../tools/sion_debug -lsiondebug
 LIBS_SOCKET += -L../tools/sion_debug -lsiondebug
endif

OBJS=ldcs_audit_server_client_cb.o ldcs_audit_server_server_cb.o ldcs_audit_server_par.o ldcs_audit_server_process.o ldcs_audit_server_filemngt.o ldcs_audit_server_stateloop.o
all: default 

default: ldcs_audit_server_socket ldcs_audit_server_pipe ldcs_audit_server_par_pipe ldcs_audit_server_par_pipe_md ldcs_audit_server_par_pipe_msocket

# serial server
ldcs_audit_server_socket:   ldcs_audit_server.o
	 $(CC) $(CFLAGS) -o ldcs_audit_server_socket ldcs_audit_server.o  $(LDFLAGS) $(LIBS_SOCKET)

ldcs_audit_server_pipe:   ldcs_audit_server.o
	 $(CC) $(CFLAGS) -o ldcs_audit_server_pipe ldcs_audit_server.o  $(LDFLAGS) $(LIBS_PIPE)

ldcs_audit_server.o: ldcs_audit_server.c Makefile ../lib/libldcs_socket.a ../lib/libldcs_pipe.a ../cache/libldcs_cache.a
	$(CC) $(CFLAGS) -c  ldcs_audit_server.c	

# parallel server
ldcs_audit_server_par.o: ldcs_audit_server_par.c Makefile ../lib/libldcs_socket.a ../lib/libldcs_pipe.a ../cache/libldcs_cache.a
	$(CC) $(CFLAGS) -c  ldcs_audit_server_par.c	

ldcs_audit_server_process.o: 	ldcs_audit_server_process.c ldcs_audit_server_process.h Makefile ../lib/libldcs_socket.a ../lib/libldcs_pipe.a ../cache/libldcs_cache.a
	$(CC) $(CFLAGS) -c  ldcs_audit_server_process.c	

ldcs_audit_server_stateloop.o: 	ldcs_audit_server_stateloop.c ldcs_audit_server_stateloop.h Makefile ../lib/libldcs_socket.a ../lib/libldcs_pipe.a ../cache/libldcs_cache.a
	$(CC) $(CFLAGS) -c  ldcs_audit_server_stateloop.c	

ldcs_audit_server_client_cb.o: 	ldcs_audit_server_client_cb.c Makefile ../lib/libldcs_socket.a ../lib/libldcs_pipe.a ../cache/libldcs_cache.a
	$(CC) $(CFLAGS) -c  ldcs_audit_server_client_cb.c	

ldcs_audit_server_server_cb.o: 	ldcs_audit_server_server_cb.c Makefile ../lib/libldcs_socket.a ../lib/libldcs_pipe.a ../cache/libldcs_cache.a
	$(CC) $(CFLAGS) -c  ldcs_audit_server_server_cb.c	

ldcs_audit_server_md_none.o: 	ldcs_audit_server_md_none.c ldcs_audit_server_md.h Makefile ldcs_audit_server_process.o ../cache/libldcs_cache.a
	$(CC) $(CFLAGS) -c  ldcs_audit_server_md_none.c	

ldcs_audit_server_filemngt.o: 	ldcs_audit_server_filemngt.c 
	$(CC) $(CFLAGS) -c  ldcs_audit_server_filemngt.c	


ldcs_audit_server_par_pipe:   ldcs_audit_server_md_none.o  $(OBJS)
	 $(CC) $(CFLAGS) -o ldcs_audit_server_par_pipe ldcs_audit_server_md_none.o $(OBJS)  $(LDFLAGS) $(LIBS_PIPE)


# parallel multi-daemon server (cobo)
ldcs_audit_server_md_cobo.o: 	ldcs_audit_server_md_cobo.c ldcs_audit_server_md.h Makefile ../cache/libldcs_cache.a
	$(CC) $(CFLAGS) $(MD_CFLAGS_COBO) -c  ldcs_audit_server_md_cobo.c	

ldcs_audit_server_par_pipe_md:  ldcs_audit_server_md_cobo.o $(OBJS)
	 $(CC) $(CFLAGS) $(MD_CFLAGS_COBO)  -o ldcs_audit_server_par_pipe_md ldcs_audit_server_md_cobo.o $(OBJS)  $(LDFLAGS) $(LIBS_PIPE) $(MD_LDFLAGS_COBO)

# parallel multi-daemon server (msocket)
ldcs_audit_server_md_msocket_util.o: 	ldcs_audit_server_md_msocket_util.c ldcs_audit_server_md.h Makefile
	$(CC) $(CFLAGS) $(MD_CFLAGS_MSOCKET) -c  ldcs_audit_server_md_msocket_util.c	

ldcs_audit_server_md_msocket_topo.o: 	ldcs_audit_server_md_msocket_topo.c ldcs_audit_server_md.h Makefile
	$(CC) $(CFLAGS) $(MD_CFLAGS_MSOCKET) -c  ldcs_audit_server_md_msocket_topo.c	

ldcs_audit_server_md_msocket.o: 	ldcs_audit_server_md_msocket.c ldcs_audit_server_md.h Makefile ../cache/libldcs_cache.a ldcs_audit_server_md_msocket_util.h
	$(CC) $(CFLAGS) $(MD_CFLAGS_MSOCKET) -c  ldcs_audit_server_md_msocket.c	

ldcs_audit_server_par_pipe_msocket:  ldcs_audit_server_md_msocket.o $(OBJS) ldcs_audit_server_md_msocket_util.h ldcs_audit_server_md_msocket_topo.h ldcs_audit_server_md_msocket.h ldcs_audit_server_md_msocket_util.o ldcs_audit_server_md_msocket_topo.o
	 $(CC) $(CFLAGS) $(MD_CFLAGS_MSOCKET)  -o ldcs_audit_server_par_pipe_msocket ldcs_audit_server_md_msocket.o ldcs_audit_server_md_msocket_util.o ldcs_audit_server_md_msocket_topo.o $(OBJS)  $(LDFLAGS) $(LIBS_PIPE) $(LIBS_SOCKET_WO_WRAPPER) $(MD_LDFLAGS_MSOCKET)

clean::
	rm -rf *.o  *~ *core*
	rm -rf ldcs_audit_server_pipe ldcs_audit_server_socket  ldcs_audit_server_par_pipe ldcs_audit_server_par_pipe_md _debug_audit*

