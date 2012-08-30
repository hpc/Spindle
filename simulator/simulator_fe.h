#ifndef LDCS_SIMULATOR_FE_H
#define LDCS_SIMULATOR_FE_H

int simulator_fe ( MPI_Comm mycomm, int num_server,
		   char  *location, int locmod, int number, MPI_Comm mycomm_FE_CL );

#endif
