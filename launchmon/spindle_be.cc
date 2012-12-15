#include <lmon_api/common.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <lmon_api/common.h>
#include <lmon_api/lmon_proctab.h>
#include <lmon_api/lmon_be.h>

#include "config.h"
#include "spindle_usrdata.h"
#include "spindle_external_fabric.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "ldcs_audit_server_md.h"
#include "ldcs_api.h"
#ifdef __cplusplus
}
#endif

static int rank, size;

int _ready_cb_func (  void * data) {
  int rc=0;
  spindle_external_fabric_data_t *spindle_external_fabric_data = (spindle_external_fabric_data_t *) data;
  MPIR_PROCDESC_EXT *proctab;
  int proctab_size;
  lmon_rc_e lrc;
  int signum, i;

#if defined(os_bg)
  signum = 0;
#elif defined(os_linux)
  signum = SIGCONT;
#else
#error Unknown OS
#endif

  lrc = LMON_be_getMyProctabSize(&proctab_size);
  if (lrc != LMON_OK)
  {
     fprintf(stderr,
             "[LMON BE(%d)] FAILED: LMON_be_getMyProctabSize\n",spindle_external_fabric_data->md_rank);
     LMON_be_finalize();
     return EXIT_FAILURE;
  }
  
  proctab = (MPIR_PROCDESC_EXT *) malloc (proctab_size*sizeof(MPIR_PROCDESC_EXT));
  if ( proctab == NULL )  {
     fprintf (stderr, 
              "[LMON BE(%d): FAILED] malloc return null\n",spindle_external_fabric_data->md_rank);
     LMON_be_finalize();
     return EXIT_FAILURE;
  }

  lrc = LMON_be_getMyProctab(proctab, &proctab_size, proctab_size);
  if (lrc != LMON_OK)   {    
    fprintf(stderr, "[LMON BE(%d): FAILED] LMON_be_getMyProctab\n",spindle_external_fabric_data->md_rank );
    LMON_be_finalize();
    return EXIT_FAILURE;
  }

  /* Continue application tasks */
  for(i=0; i < proctab_size; i++)  {
    debug_printf("[LMON BE(%d)] kill %d, %d\n", spindle_external_fabric_data->md_rank, proctab[i].pd.pid, signum );
    kill(proctab[i].pd.pid, signum);
  }
  
  for (i=0; i < proctab_size; i++) {
    if (proctab[i].pd.executable_name) free(proctab[i].pd.executable_name);
    if (proctab[i].pd.host_name)       free(proctab[i].pd.host_name);
  }
  free (proctab);

  return(rc);
}

int main(int argc, char* argv[])
{
  lmon_rc_e lrc;
  spindle_external_fabric_data_t spindle_external_fabric_data;
  int number;
  char *location, *numberstr;

/*  fprintf(stderr, "Here - %d\n", getpid());
  static volatile int done = 0;
  while (!done) {     
     sleep(1);
     }*/

  /* Initialization of LMON  */
  lrc = LMON_be_init(LMON_VERSION, &argc, &argv);
  if (lrc != LMON_OK)
  {      
     fprintf(stdout, 
             "[LMON BE: FAILED] LMON_be_init\n");
     return EXIT_FAILURE;
  }

  location  = argv[1];
  numberstr = argv[2];
  number = numberstr ? atoi(numberstr) : -1;

  LMON_be_getMyRank(&rank);
  LMON_be_getSize(&size);

  lrc = LMON_be_handshake(NULL);
  if (lrc != LMON_OK)
  {
     fprintf(stdout, "[LMON BE(%d)] FAILED: LMON_be_handshake\n", rank);
     LMON_be_finalize();
     return EXIT_FAILURE;
  }

  lrc = LMON_be_ready(NULL);
  if (lrc != LMON_OK )
  {     
     fprintf(stdout, "[LMON BE(%d)] FAILED: LMON_be_ready\n", rank);
     LMON_be_finalize();
     return EXIT_FAILURE;
  } 

  /* Register pack/unpack functions to LMON  */
  if ( LMON_be_amIMaster() == LMON_YES ) {
    
    if ( ( lrc = LMON_be_regPackForBeToFe (packbefe_cb )) != LMON_OK ) {
      fprintf (stdout, "[LMON BE(%d)] LMON_be_regPackForBeToFe FAILED\n",  rank);
      return EXIT_FAILURE;
    } 
    
    if ( ( lrc = LMON_be_regUnpackForFeToBe ( unpackfebe_cb )) != LMON_OK ) {
      fprintf (stdout,"[LMON BE(%d)] LMON_be_regUnpackForFeToBe FAILED\n", rank);
      return EXIT_FAILURE;
    } 
  }

  /* set SION debug file name */
  if(1){
    char hostname[HOSTNAME_LEN];
    bzero(hostname,HOSTNAME_LEN);
    gethostname(hostname,HOSTNAME_LEN);

    char helpstr[MAX_PATH_LEN];
    sprintf(helpstr,"_debug_spindle_%s_l_%02d_of_%02d_%s","DAEMON",rank+1,size,hostname);
    setenv("SION_DEBUG",helpstr,1);
  }

  /* Register external fabric CB to SPINDLE */
  spindle_external_fabric_data.md_rank=rank;
  spindle_external_fabric_data.md_size=size;
  ldcs_register_external_fabric_CB( &spindle_external_fabric_be_CB, (void *) &spindle_external_fabric_data);

  /* start SPINDLE server */
  ldcs_audit_server_process(location,number,&_ready_cb_func, &spindle_external_fabric_data);

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
