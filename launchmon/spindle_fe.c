#define TARGET_JOB_LAUNCHER_PATH_srun "/usr/bin/srun"
#define TARGET_JOB_LAUNCHER_PATH_orte "/usr/lib/mpi/gcc/openmpi/bin/orterun"
#define RM_ORTE_ORTERUN 1

/* #define RM_SLURM_SRUN 1 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <lmon_api/common.h>
#include <lmon_api/lmon_proctab.h>
#include <lmon_api/lmon_fe.h>
#include <sys/time.h>

#include "ldcs_audit_server_md.h"
#include "sample_usrdata.h"

#include "spindle_external_fabric.h"

const int MAXPROCOUNT  = 12000;

/*
 * OUR PARALLEL JOB LAUNCHER 
 */
const char* mylauncher    = TARGET_JOB_LAUNCHER_PATH_orte;

double __get_time()
{
  struct timeval tp;
  gettimeofday (&tp, (struct timezone *)NULL);
  return tp.tv_sec + tp.tv_usec/1000000.0;
}

int 
main (int argc, char* argv[])
{  
  int aSession    = 0;
  char **launcher_argv        = NULL;
  char **daemon_opts          = NULL;
  
  lmon_rc_e rc, lrc;

  ldcs_host_port_list_t host_port_list;
  void *md_data_ptr;
  spindle_external_fabric_data_t spindle_external_fabric_data;
  int i;

  double starttime=__get_time();
  double steptime=__get_time();
  
  char preloadfile[255];

  for (i=0;i<argc;i++) {
    fprintf ( stdout, "FE parm[%d]=%s\n",i,argv[i]);
   
  }

  bzero(preloadfile,255);
  strncpy(preloadfile,"preload_files.dat",254);

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
        "%s cannot be executed (1)\n", 
        argv[1] );
      fprintf ( stdout, 
        "[LMON FE] FAILED\n" );
      return EXIT_FAILURE;     
    }

  if ( access(argv[5], X_OK) < 0 )
    {
      fprintf(stdout, 
        "%s cannot be executed (5)\n", 
        argv[5]);
      fprintf(stdout, 
        "[LMON FE] FAILED\n");
      return EXIT_FAILURE;	      
    }
  if ( argc > 6 )
    daemon_opts = argv+6;

#if RM_SLURM_SRUN
  {
    char partition_opt[255];
    char numprocs_opt[255];
    char numnodes_opt[255];
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
  
  printf("SERVERFE: start of process startup at %10.6f (%6.3f s from start)\n", 
	 __get_time(),__get_time()-starttime);

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

  /* set SION debug file name */
  if(1){
    char hostname[HOSTNAME_LEN];
    bzero(hostname,HOSTNAME_LEN);
    gethostname(hostname,HOSTNAME_LEN);

    char helpstr[MAX_PATH_LEN];
    sprintf(helpstr,"_debug_spindle_%s_l_%02d_of_%02d_%s","FE",1,1,hostname);
    setenv("SION_DEBUG",helpstr,1);
    printf("SIMULATOR: set SION_DEBUG=%s\n",helpstr);
  }
  
  printf("SERVERFE: end   of process startup at %10.6f (%6.3f s from start)\n", __get_time(),__get_time()-starttime);

  /* Register external fabric CB to SPINDLE */
  spindle_external_fabric_data.md_rank=-1;
  spindle_external_fabric_data.md_size=-1;
  spindle_external_fabric_data.asession=aSession;
  ldcs_register_external_fabric_CB( &spindle_external_fabric_fe_CB, (void *) &spindle_external_fabric_data);



  /* SPINDLE FE start */
  printf("SERVERFE: start of audit_server_open at %10.6f (%6.3f s from start)\n", __get_time(),__get_time()-starttime);
  ldcs_audit_server_fe_md_open( NULL, -1, &md_data_ptr );
  printf("SERVERFE: end of audit_server_open    LDCS_FE_OPEN    time = %14.4f\n", __get_time()-steptime);
  printf("SERVERFE: end   of audit_server_open at %10.6f (%6.3f s from start)\n", __get_time(),__get_time()-starttime);


  printf("SERVERFE: start of audit_server_preload(%s) at %10.6f (%6.3f s from start)\n", preloadfile,__get_time(),__get_time()-starttime);
  steptime=__get_time();
  ldcs_audit_server_fe_md_preload(preloadfile, md_data_ptr );
  printf("SERVERFE: end of audit_server_preload LDCS_FE_PRELOAD time = %14.4f\n", __get_time()-steptime);
  printf("SERVERFE: end   of audit_server_preload at %10.6f (%6.3f s from start)\n", __get_time(),__get_time()-starttime);
  
  printf("SERVERFE: SERVER_STARTUP_READY\n");
  fprintf(stderr,"SERVERFE: SERVER_STARTUP_READY\n");


  /* Close the server, implicit wait */
  steptime=__get_time();
  ldcs_audit_server_fe_md_close(md_data_ptr);
  printf("SERVERFE: end of audit_server_close   LDCS_FE_CLOSE   time = %14.4f\n", __get_time()-steptime);
  
  printf("SERVERFE: end of audit_server_close at %10.6f (%6.3f s from start)\n", 
	 __get_time(),__get_time()-starttime);

  /* Waiting for end of SPINDLE daemons */
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
