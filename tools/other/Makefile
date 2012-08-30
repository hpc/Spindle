OPT=
DEBUG=NODEBUG
DEBUGD=-D$(DEBUG)

CFLAGS=  -g $(OPT)  $(DEBUGD) -I../lib
FFLAGS=  -g $(OPT)
LDFLAGS = -g $(OPT) 

LIBS = -lrt
LIBS = 

MPICC = mpicc


ifeq ($(DEBUG),SIONDEBUG)
 CFLAGS += -I../../tools/sion_debug
 SOCFLAGS += -I../../tools/sion_debug
 LIBS += -L../../tools/sion_debug -lsiondebug
 SOLIBS_SOCKET += -L../../tools/sion_debug -lsiondebug
 SOLIBS_PIPE   += -L../../tools/sion_debug -lsiondebug
endif

all: toposim helloworld_mpi

default: toposim

helloworld_mpi:   helloworld_mpi.o
	 $(MPICC) $(CFLAGS) -o helloworld_mpi helloworld_mpi.o $(LIBS) 


helloworld_mpi.o: helloworld_mpi.c Makefile
	$(MPICC) $(CFLAGS) -c  helloworld_mpi.c 	

toposim:   toposim.o
	 $(CC) $(CFLAGS) -o toposim toposim.o $(LIBS) 

toposim.o: 	toposim.c Makefile 
	$(CC) $(CFLAGS) -c  toposim.c	


clean::
	rm -rf *.o *.so toposim

