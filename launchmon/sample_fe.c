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
 *        Jun 17 2008 DHA: Added BlueGene support 
 *        Jun 12 2008 DHA: Added GNU build system support
 *        Feb 09 2008 DHA: Added LLNS Copyright. 
 *        Aug 06 2007 DHA: Created file   
 */

#define TARGET_JOB_LAUNCHER_PATH_srun "/usr/bin/srun"
#define TARGET_JOB_LAUNCHER_PATH_orte "/usr/lib/mpi/gcc/openmpi/bin/orterun"
#define RM_ORTE_ORTERUN 1

#include <unistd.h>
#include <limits.h>
#include <lmon_api/common.h>
#include <lmon_api/lmon_proctab.h>
#include <lmon_api/lmon_fe.h>

#include "sample_usrdata.h"

const int MAXPROCOUNT  = 12000;

/*
 * OUR PARALLEL JOB LAUNCHER 
 */
const char* mylauncher    = TARGET_JOB_LAUNCHER_PATH_orte;


int 
main (int argc, char* argv[])
{  
  int aSession    = 0;
  char **launcher_argv        = NULL;
  char **daemon_opts          = NULL;
  
  lmon_rc_e rc, lrc;

  ldcs_host_port_list_t host_port_list;
  int i;

  if ( argc < 6 )
    {
      fprintf ( stdout, 
		"Usage: sample_fe appcode numprocs numnodes partition daemonpath [daemonargs]\n" );
      fprintf ( stdout, 
		"[LMON FE] FAILED\n" );
      return EXIT_FAILURE;	      
    }

  if ( access(argv[1], X_OK) < 0 )
    {
      fprintf ( stdout, 
		"%s cannot be executed\n", 
		argv[1] );
      fprintf ( stdout, 
		"[LMON FE] FAILED\n" );
      return EXIT_FAILURE;     
    }

  if ( access(argv[5], X_OK) < 0 )
    {
      fprintf(stdout, 
	      "%s cannot be executed\n", 
	      argv[2]);
      fprintf(stdout, 
	      "[LMON FE] FAILED\n");
      return EXIT_FAILURE;	      
    }
  if ( argc > 6 )
    daemon_opts = argv+6;

#if RM_SLURM_SRUN
  {
    char numprocs_opt[255];
    char numnodes_opt[255];
    char partition_opt[255];

    sprintf(numprocs_opt, "-n %s",argv[2]);
    sprintf(numnodes_opt, "-N %s",argv[3]);
    sprintf(partition_opt, "-p %s",argv[4]);
   
    launcher_argv = (char**) malloc(7*sizeof(char*));
    launcher_argv[0] = strdup(mylauncher);
    launcher_argv[1] = strdup(numprocs_opt);
    launcher_argv[2] = strdup(numnodes_opt);
    launcher_argv[3] = strdup(partition_opt);
    launcher_argv[4] = strdup("-l");
    launcher_argv[5] = strdup(argv[1]);
    launcher_argv[6] = NULL;
  }
#elif RM_BG_MPIRUN 
  launcher_argv = (char**) malloc(8*sizeof(char*));
  launcher_argv[0] = strdup(mylauncher);
  launcher_argv[1] = strdup("-verbose");
  launcher_argv[2] = strdup("1");
  launcher_argv[3] = strdup("-np");
  launcher_argv[4] = strdup(argv[2]);
  launcher_argv[5] = strdup("-exe");
  launcher_argv[6] = strdup(argv[1]);
  launcher_argv[7] = NULL;
  fprintf (stdout, "[LMON FE] launching the job/daemons via %s\n", mylauncher);
#elif RM_ALPS_APRUN
  numprocs_opt     = string("-n") + string(argv[2]);
  launcher_argv    = (char**) malloc(4*sizeof(char*));
  launcher_argv[0] = strdup(mylauncher);
  launcher_argv[1] = strdup(numprocs_opt.c_str());
  launcher_argv[2] = strdup(argv[1]);
  launcher_argv[3] = NULL;
#elif RM_ORTE_ORTERUN
  launcher_argv    = (char **) malloc(8*sizeof(char*));
  launcher_argv[0] = strdup(mylauncher);
  launcher_argv[1] = strdup("-mca");
  launcher_argv[2] = strdup("debugger");
  launcher_argv[3] = strdup("mpirx");
  launcher_argv[4] = strdup("-np");
  launcher_argv[5] = strdup(argv[2]);
  launcher_argv[6] = strdup(argv[1]);
  launcher_argv[7] = NULL;
  fprintf (stdout, "[LMON_FE] launching the job/daemons via %s\n", mylauncher);
#else
# error add support for the RM of your interest here
#endif

  if ( ( rc = LMON_fe_init ( LMON_VERSION ) ) 
       != LMON_OK )  {
    fprintf ( stdout, "[LMON FE] LMON_fe_init FAILED\n" );
    return EXIT_FAILURE;
  }
  
  if ( ( rc = LMON_fe_createSession (&aSession)) 
       != LMON_OK)  {
    fprintf ( stdout,   "[LMON FE] LMON_fe_createFEBESession FAILED\n");
    return EXIT_FAILURE;
  }

  if ( ( lrc = LMON_fe_regPackForFeToBe (aSession, packfebe_cb )) != LMON_OK ) {
    fprintf (stdout, "[LMON FE] LMON_fe_regPackForFeToBe FAILED\n");
    return EXIT_FAILURE;
  } 
  
  if ( ( lrc = LMON_fe_regUnpackForBeToFe (aSession, unpackfebe_cb )) != LMON_OK ) {
    fprintf (stdout,"[LMON FE] LMON_fe_regUnpackForBeToFe FAILED\n");
    return EXIT_FAILURE;
  } 

  fprintf ( stdout, "[LMON FE] LMON_fe_launchAndSpawnDaemons(\"%s %s %s %s %s %s\")\n",launcher_argv[0]
	    ,launcher_argv[1],launcher_argv[2],launcher_argv[3],launcher_argv[4],launcher_argv[5]);
  
  if ( ( rc = LMON_fe_launchAndSpawnDaemons ( 
					     aSession, 
					     NULL,
					     launcher_argv[0],
					     launcher_argv,
					     argv[5],
					     daemon_opts,
					     NULL,
					     NULL)) 
       != LMON_OK ) {
    fprintf ( stdout, "[LMON FE] LMON_fe_launchAndSpawnDaemons FAILED\n" );
    return EXIT_FAILURE;
  }
  

  if ( ( lrc = LMON_fe_recvUsrDataBe ( aSession, (void*) &host_port_list )) != LMON_OK )    {
    fprintf(stdout, "[LMON FE: FAILED] LMON_be_sendUsrData\n" );
    return EXIT_FAILURE;
  }
  
  for(i=0; i < host_port_list.size; i++)  {
    printf("[LMON FE] HOSTLIST[%d] %s, %d\n", i, &host_port_list.hostlist[i*HOSTNAME_LEN], host_port_list.portlist[i] );
  }
  

  rc = LMON_fe_recvUsrDataBe ( aSession, NULL );
  if ( (rc == LMON_EBDARG )
       || ( rc == LMON_ENOMEM )
       || ( rc == LMON_EINVAL ) )  {
    fprintf ( stdout, "[LMON FE] FAILED\n");
    return EXIT_FAILURE;
  }

  sleep (3);

  fprintf ( stdout,
	    "\n[LMON FE] PASS: run through the end\n");
  
  return EXIT_SUCCESS;
}
