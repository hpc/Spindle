/*
 * $Header: $
 *--------------------------------------------------------------------------------
 * Copyright (c) 2008, Lawrence Livermore National Security, LLC. Produced at 
 * the Lawrence Livermore National Laboratory. Written by Dong H. Ahn <ahn1@llnl.gov>. 
 * LLNL-CODE-409469. All rights reserved.
 *
 * This file is part of LaunchMON. For details, see 
 * https://computing.llnl.gov/?set=resources&page=os_projects
 *
 * Please also read LICENSE.txt -- Our Notice and GNU Lesser General Public License.
 *
 * 
 * This program is free software; you can redistribute it and/or modify it under the 
 * terms of the GNU General Public License (as published by the Free Software 
 * Foundation) version 2.1 dated February 1999.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 59 Temple 
 * Place, Suite 330, Boston, MA 02111-1307 USA
 *--------------------------------------------------------------------------------			
 *
 *  Update Log:
 *        Mar 04 2008 DHA: Added generic BlueGene support
 *        Jun 12 2008 DHA: Added GNU build system support
 *        Feb 09 2008 DHA: Added LLNS Copyright.
 *        Aug 06 2007 DHA: Created file.
 *          
 */

#include <lmon_api/common.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <lmon_api/lmon_be.h>

#include "sample_usrdata.h"


int 
main( int argc, char* argv[] )
{
  MPIR_PROCDESC_EXT *proctab;
  int proctab_size;
  int signum;
  int i, rank, size;
  lmon_rc_e lrc;

#if RM_BG_MPIRUN
  signum = 0;
#else
  signum = SIGCONT;
#endif

  fprintf(stdout, "[LMON BE: ] starting LMON_be_init\n");

  if ( (lrc = LMON_be_init(LMON_VERSION, &argc, &argv)) 
       != LMON_OK )
    {      
      fprintf(stdout, 
	      "[LMON BE: FAILED] LMON_be_init\n");
      return EXIT_FAILURE;
    }
  fprintf(stdout, "[LMON BE: ] finished LMON_be_init\n");

  if (argc > 1) 
    signum = atoi(argv[1]);

  LMON_be_getMyRank (&rank);
  LMON_be_getSize (&size);

  if ( (lrc = LMON_be_handshake(NULL)) 
       != LMON_OK )
    {
      fprintf(stdout, 
	      "[LMON BE(%d)] FAILED: LMON_be_handshake\n",
	      rank );
      LMON_be_finalize();
      return EXIT_FAILURE;
    }
  fprintf(stdout, "[LMON BE: ] finished LMON_be_handshake\n");

  if ( (lrc = LMON_be_ready(NULL)) 
       != LMON_OK )
    {     
      fprintf(stdout, 
	      "[LMON BE(%d)] FAILED: LMON_be_ready\n",
	      rank );
      LMON_be_finalize();
      return EXIT_FAILURE;
    } 

  fprintf(stdout, "[LMON BE: ] finished LMON_be_ready\n");

  if ( (lrc = LMON_be_getMyProctabSize(&proctab_size)) 
       != LMON_OK )
    {
      fprintf(stdout,
	      "[LMON BE(%d)] FAILED: LMON_be_getMyProctabSize\n",
	      rank);
      LMON_be_finalize();
      return EXIT_FAILURE;
    }
  
  proctab = (MPIR_PROCDESC_EXT *) 
    malloc (proctab_size*sizeof(MPIR_PROCDESC_EXT));
  if ( proctab == NULL )  {
    fprintf (stdout, 
	     "[LMON BE(%d): FAILED] malloc return null\n",
	     rank);
    LMON_be_finalize();
    return EXIT_FAILURE;
  }

  if ( (lrc = LMON_be_getMyProctab(proctab, &proctab_size, proctab_size)) 
       != LMON_OK )   {    
    fprintf(stdout, 
	    "[LMON BE(%d): FAILED] LMON_be_getMyProctab\n", 
	    rank );
    LMON_be_finalize();
    return EXIT_FAILURE;
  }
  
  for(i=0; i < proctab_size; i++) {
    fprintf(stdout, 
	    "[LMON BE(%d)] Target process: %8d, MPI RANK: %5d\n", 
	    rank,
	    proctab[i].pd.pid, 
	    proctab[i].mpirank);
  }

  fprintf(stdout, "[LMON BE: ] finished LMON_be_getMyProctab\n");


  /* send hostname to FE */
  {  
    char hostname[HOSTNAME_LEN];
    int port=5000+rank;
    ldcs_host_port_list_t host_port_list;

    bzero(hostname,HOSTNAME_LEN);
    gethostname(hostname,HOSTNAME_LEN);


    if ( LMON_be_amIMaster() == LMON_YES ) {
      host_port_list.size=size;
      host_port_list.hostlist=(char *) malloc(size*HOSTNAME_LEN);
      host_port_list.portlist=(int *) malloc(size*sizeof(int));
    } else {
      host_port_list.size=0;
      host_port_list.hostlist=NULL;
      host_port_list.portlist=NULL;
    }

    if (( lrc = LMON_be_gather ( hostname, HOSTNAME_LEN, host_port_list.hostlist )) != LMON_OK) {
      fprintf(stdout,     "[LMON BE(%d)] FAILED: LMON_be_gather\n",  rank );
      LMON_be_finalize();
      return EXIT_FAILURE;
    }

    if (( lrc = LMON_be_gather ( &port, sizeof(int), host_port_list.portlist )) != LMON_OK) {
      fprintf(stdout,     "[LMON BE(%d)] FAILED: LMON_be_gather\n",  rank );
      LMON_be_finalize();
      return EXIT_FAILURE;
    }
    
    if ( LMON_be_amIMaster() == LMON_YES ) {

      for(i=0; i < size; i++)  {
	printf("[LMON BE(%d)] HOSTLIST[%d] %s, %d\n", rank, i, &host_port_list.hostlist[i*HOSTNAME_LEN], host_port_list.portlist[i] );
      }

      if ( ( lrc = LMON_be_regPackForBeToFe (packbefe_cb )) != LMON_OK ) {
	  fprintf (stdout, "[LMON BE(%d)] LMON_be_regPackForBeToFe FAILED\n",  rank);
	  return EXIT_FAILURE;
	} 

      if ( ( lrc = LMON_be_regUnpackForFeToBe ( unpackfebe_cb )) != LMON_OK ) {
	fprintf (stdout,"[LMON BE(%d)] LMON_be_regUnpackForFeToBe FAILED\n", rank);
	return EXIT_FAILURE;
      } 

      if ( ( lrc = LMON_be_sendUsrData ( (void*) &host_port_list )) != LMON_OK )    {
	fprintf(stdout, "[LMON BE(%d): FAILED] LMON_be_sendUsrData\n", rank );
	LMON_be_finalize();
	return EXIT_FAILURE;
      }

    }
      
  }
  
  for(i=0; i < proctab_size; i++)  {
    printf("[LMON BE(%d)] kill %d, %d\n", rank, proctab[i].pd.pid, signum );
    kill(proctab[i].pd.pid, signum);
  }
  
  for (i=0; i < proctab_size; i++) {
    if (proctab[i].pd.executable_name) free(proctab[i].pd.executable_name);
    if (proctab[i].pd.host_name)       free(proctab[i].pd.host_name);
  }
  free (proctab);

  fprintf(stdout, "[LMON BE: ] finished --> signal\n");


  /* sending this to mark the end of the BE session */
  /* This should be used to determine PASS/FAIL criteria */
  if ( (( lrc = LMON_be_sendUsrData ( NULL )) == LMON_EBDARG)
       || ( lrc == LMON_EINVAL )
       || ( lrc == LMON_ENOMEM ))
    {
      fprintf(stdout, "[LMON BE(%d)] FAILED(%d): LMON_be_sendUsrData\n",
	      rank, lrc );
      LMON_be_finalize();
      return EXIT_FAILURE;
    }

  fprintf(stdout, "[LMON BE: ] starting LMON_be_finalize \n");
  LMON_be_finalize();
  fprintf(stdout, "[LMON BE: ] finished LMON_be_finalize \n");

  return EXIT_SUCCESS;
}
