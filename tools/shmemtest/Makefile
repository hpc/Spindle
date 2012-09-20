DEBUG=NODEBUG
DEBUGD=-D$(DEBUG)
CFLAGS=  -g $(DEBUGD) -I../../comlib
FFLAGS=  -g
LIBS  = -g -lrt
LIBS_SHMEM  = -g -L../../comlib -lldcs_shmem_wo_wrapper  -lrt
LIBTOOL_COMP=libtool --mode compile --tag CC
LIBTOOL_LINK=libtool --mode link  --tag CC

MPICC = mpicc

ifeq ($(DEBUG),SIONDEBUG)
 CFLAGS      += -I../sion_debug
 LIBS        += -L../sion_debug -lsiondebug
 LIBS_SHMEM  += -L../sion_debug -lsiondebug
endif

all: default 

default: shmem_test_mpi ldcs_shmem_test_mpi

shmem_test_mpi:   shmem_test_mpi.o
	 $(MPICC) $(CFLAGS) -o shmem_test_mpi shmem_test_mpi.o $(LIBS) 

ldcs_shmem_test_mpi:   ldcs_shmem_test_mpi.o
	 $(LIBTOOL_LINK) $(MPICC) $(CFLAGS) -o ldcs_shmem_test_mpi ldcs_shmem_test_mpi.o $(LIBS_SHMEM) 

shmem_test_mpi.o: 	shmem_test_mpi.c Makefile
	$(MPICC) $(CFLAGS) -c  shmem_test_mpi.c	

ldcs_shmem_test_mpi.o: 	ldcs_shmem_test_mpi.c Makefile
	$(LIBTOOL_COMP) $(MPICC) $(CFLAGS) -c  ldcs_shmem_test_mpi.c	

clean::
	rm -rf *.o  *~ *core*
	rm -rf shmem_test_mpi

