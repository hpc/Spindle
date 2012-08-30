OPT=
DEBUG=NODEBUG
DEBUGD=-D$(DEBUG)
CFLAGS= -g $(OPT) $(DEBUGD) -I. -I../lib -Wall -fPIC
FFLAGS= -g $(OPT)
LDFLAGS = -g $(OPT) 
LIBS = 
C_AR= ar 

ifeq ($(DEBUG),SIONDEBUG)
 CFLAGS += -I../../../tools/sion_debug
 LIBS += -L../../../tools/sion_debug -lsiondebug
endif

LDCS_OBJS =  cobo.o 
LDCS_COMM_OBJS =  cobo_comm.o 

all: default 

default: libcobo.a libcobocomm.a

libcobo.a :  $(LDCS_OBJS)
	$(C_AR) $(AFLAG) -rcs $@ $(LDCS_OBJS)
	@echo "*** $@ built"

cobo.o: cobo.c cobo.h Makefile
	$(CC) $(CFLAGS) -c  cobo.c	

libcobocomm.a :  $(LDCS_COMM_OBJS)
	$(C_AR) $(AFLAG) -rcs $@ $(LDCS_COMM_OBJS)
	@echo "*** $@ built"

cobo_comm.o: cobo_comm.c cobo_comm.h Makefile
	$(CC) $(CFLAGS) -c  cobo_comm.c	

clean::
	rm -rf *.o *.a  *~ *core*
