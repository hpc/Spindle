DEBUG=NODEBUG
DEBUGD=-D$(DEBUG)
CFLAGS=  -g $(DEBUGD) -I../../lib
FFLAGS=  -g
LIBS  = -g -lrt
LIBS_SHMEM  = -g -L../../lib -lldcs_shmem_wo_wrapper  -lrt

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
	 $(MPICC) $(CFLAGS) -o ldcs_shmem_test_mpi ldcs_shmem_test_mpi.o $(LIBS_SHMEM) 

shmem_test_mpi.o: 	shmem_test_mpi.c Makefile
	$(MPICC) $(CFLAGS) -c  shmem_test_mpi.c	

ldcs_shmem_test_mpi.o: 	ldcs_shmem_test_mpi.c Makefile
	$(MPICC) $(CFLAGS) -c  ldcs_shmem_test_mpi.c	

clean::
	rm -rf *.o  *~ *core*
	rm -rf shmem_test_mpi

