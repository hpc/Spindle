OPT=
DEBUG=NODEBUG
DEBUGD=-D$(DEBUG)
CFLAGS= -g $(OPT) $(DEBUGD) -I. -I../lib -Wall
FFLAGS= -g $(OPT)
LDFLAGS = -g $(OPT)
LIBS = 
C_AR= ar 

ifeq ($(DEBUG),SIONDEBUG)
 CFLAGS += -I../tools/sion_debug
 LIBS += -L../tools/sion_debug -lsiondebug
endif

LDCS_OBJS =  ldcs_cache.o ldcs_cache_util.o ldcs_cache_file_op.o ldcs_hash.o

all: default 

default: libldcs_cache.a

libldcs_cache.a :  $(LDCS_OBJS)
	$(C_AR) $(AFLAG) -rcs $@ ldcs_cache.o $(LDCS_OBJS)
	@echo "*** $@ built"

ldcs_cache.o: 	ldcs_cache.c ldcs_cache.h Makefile
	$(CC) $(CFLAGS) -c  ldcs_cache.c	

ldcs_cache_util.o: 	ldcs_cache_util.c ldcs_cache.h Makefile
	$(CC) $(CFLAGS) -c  ldcs_cache_util.c	

ldcs_cache_file_op.o: 	ldcs_cache_file_op.c ldcs_cache.h Makefile
	$(CC) $(CFLAGS) -c  ldcs_cache_file_op.c	

ldcs_hash.o: 	ldcs_hash.c ldcs_hash.h Makefile
	$(CC) $(CFLAGS) -c  ldcs_hash.c	

clean::
	rm -rf *.o *.a  *~ *core*
