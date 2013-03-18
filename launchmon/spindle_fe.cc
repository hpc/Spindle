#define TARGET_JOB_LAUNCHER_PATH "/usr/bin/srun"
#define RM_SLURM_SRUN 1

#include "lmon_api/common.h"
#include "lmon_api/lmon_proctab.h"
#include "lmon_api/lmon_fe.h"
#include "parse_launcher.h"
#include "spindle_usrdata.h"
#include "spindle_external_fabric.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "ldcs_audit_server_md.h"

#ifdef __cplusplus
}
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <sys/time.h>
#include <sys/types.h>

#include <set>
#include <algorithm>
#include <string>
#include <iterator>
#include <pthread.h>

using namespace std;

#if !defined(BINDIR)
#error Expected BINDIR to be defined
#endif

char spindle_bootstrap[] = BINDIR "/spindle_bootstrap";
char spindle_daemon[] = BINDIR "/spindle_be";

const int MAXPROCOUNT  = 12000;

#define DEFAULT_LDCS_NAME_PREFIX "/tmp/"
#define DEFAULT_LDCS_NUMBER 7777

static char *ldcs_location = NULL;
static unsigned int ldcs_number = 0;

/*
 * OUR PARALLEL JOB LAUNCHER 
 */
const char* mylauncher    = TARGET_JOB_LAUNCHER_PATH;

double __get_time()
{
  struct timeval tp;
  gettimeofday (&tp, (struct timezone *)NULL);
  return tp.tv_sec + tp.tv_usec/1000000.0;
}

static char *getUserName()
{
   uid_t uid = geteuid();
   struct passwd *pwid;
   do {
      pwid = getpwuid(uid);
   } while (!pwid && errno == EINTR);
   if (!pwid) {
      perror("Failed to read username");
      return NULL;
   }
   return pwid->pw_name;
}

int parseCmdLine(int *argc, char **argv[])
{
   //TODO. No spindle arguments yet
   return 0;
}

void usage() {
   fprintf(stderr, "Usage: spindle [mpirun|srun] ...\n");
}

static pthread_mutex_t completion_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t completion_condvar = PTHREAD_COND_INITIALIZER;
static bool spindle_done = false;

static void signal_done()
{
   pthread_mutex_lock(&completion_lock);
   spindle_done = true;
   pthread_cond_signal(&completion_condvar);
   pthread_mutex_unlock(&completion_lock);
}

static void waitfor_done()
{
  pthread_mutex_lock(&completion_lock);
  while (!spindle_done) {
     pthread_cond_wait(&completion_condvar, &completion_lock);
  }
  pthread_mutex_unlock(&completion_lock);
}

static int onLaunchmonStatusChange(int *pstatus) {
   int status = *pstatus;
   if (WIFKILLED(status) || WIFDETACHED(status)) {
      signal_done();
   }
   return 0;
}

string pt_to_string(const MPIR_PROCDESC_EXT &pt) { return pt.pd.host_name; }
const char *string_to_cstr(const std::string &str) { return str.c_str(); }

int main (int argc, char* argv[])
{  
  int aSession    = 0;
  int launcher_argc;
  char **launcher_argv        = NULL;
  //char **daemon_opts          = NULL;
  const char **daemon_opts = NULL;
  int launchers_to_use;
  int result;
  const char *bootstrapper = spindle_bootstrap;
  const char *daemon = spindle_daemon;
  char ldcs_number_s[32];
  lmon_rc_e rc;
  void *md_data_ptr;
  spindle_external_fabric_data_t spindle_external_fabric_data;

  result = parseCmdLine(&argc, &argv);
  if (result != 0) {
     usage();
     return EXIT_FAILURE;
  }

  if (strlen(LAUNCHMON_BIN_DIR)) {
     setenv("LMON_LAUNCHMON_ENGINE_PATH", LAUNCHMON_BIN_DIR "/launchmon", 0);
     setenv("LMON_PREFIX", LAUNCHMON_BIN_DIR "/..", 0);
  }

  /**
   * If number and location weren't set, then set them.
   **/
  if (!ldcs_location) {
     int name_size;
     const char *username = getUserName();
     if (!username) {
        username = "spindleuser";
     }
     name_size = strlen(username) + strlen(DEFAULT_LDCS_NAME_PREFIX) + 1;
     ldcs_location = (char *) malloc(name_size);
     snprintf(ldcs_location, name_size, "%s%s", DEFAULT_LDCS_NAME_PREFIX, username);
  }
  if (!ldcs_number) {
     ldcs_number = DEFAULT_LDCS_NUMBER;
  }
  snprintf(ldcs_number_s, 32, "%d", ldcs_number);

  /**
   * Setup the launcher command line
   **/
  launchers_to_use = TEST_PRESETUP;
  /* TODO: Add more launchers, then use autoconf to select the correct TEST_* values */
  launchers_to_use |= TEST_SLURM;
  result = createNewCmdLine(argc, argv, &launcher_argc, &launcher_argv, bootstrapper, 
                            ldcs_location, ldcs_number_s, launchers_to_use);
  if (result != 0) {
     fprintf(stderr, "Error parsing command line:\n");
     if (result == NO_LAUNCHER) {
        fprintf(stderr, "Could not find a job launcher (mpirun, srun, ...) in the command line\n");
     }
     if (result == NO_EXEC) {
        fprintf(stderr, "Could not find an executable in the given command line\n");
     }
     return EXIT_FAILURE;
  }

  /**
   * Setup the daemon command line
  **/
  daemon_opts = (const char **) malloc(4 * sizeof(char *));
  daemon_opts[0] = daemon;
  daemon_opts[1] = ldcs_location;
  daemon_opts[2] = ldcs_number_s;
  daemon_opts[3] = NULL;

  /**
   * Setup LaunchMON
   **/
  rc = LMON_fe_init(LMON_VERSION);
  if (rc != LMON_OK )  {
      fprintf ( stderr, "[LMON FE] LMON_fe_init FAILED\n" );
      return EXIT_FAILURE;
  }

  rc = LMON_fe_createSession(&aSession);
  if (rc != LMON_OK)  {
    fprintf ( stderr,   "[LMON FE] LMON_fe_createFEBESession FAILED\n");
    return EXIT_FAILURE;
  }

  rc = LMON_fe_regStatusCB(aSession, onLaunchmonStatusChange);
  if (rc != LMON_OK) {
    fprintf ( stderr,   "[LMON FE] LMON_fe_regStatusCB FAILED\n");     
    return EXIT_FAILURE;
  }
  
  rc = LMON_fe_regPackForFeToBe(aSession, packfebe_cb);
  if (rc != LMON_OK) {
    fprintf (stderr, "[LMON FE] LMON_fe_regPackForFeToBe FAILED\n");
    return EXIT_FAILURE;
  } 
  
  rc = LMON_fe_regUnpackForBeToFe(aSession, unpackfebe_cb);
  if (rc != LMON_OK) {
     fprintf (stderr,"[LMON FE] LMON_fe_regUnpackForBeToFe FAILED\n");
    return EXIT_FAILURE;
  }

  int i = 0;
  printf("launcher: ");
  for (i = 0; launcher_argv[i]; i++) {
     printf("%s ", launcher_argv[i]);
  }
  printf("\n");
  printf("daemon: ");
  for (i = 0; daemon_opts[i]; i++) {
     printf("%s ", daemon_opts[i]);
  }
  printf("\n");

  rc = LMON_fe_launchAndSpawnDaemons(aSession, NULL,
                                     launcher_argv[0], launcher_argv,
                                     daemon_opts[0], const_cast<char **>(daemon_opts+1),
                                     NULL, NULL);
  if (rc != LMON_OK) {
     fprintf(stderr, "[LMON FE] LMON_fe_launchAndSpawnDaemons FAILED\n");
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
  }
  
  /* Register external fabric CB to SPINDLE */
  spindle_external_fabric_data.md_rank=-1;
  spindle_external_fabric_data.md_size=-1;
  spindle_external_fabric_data.asession=aSession;
  ldcs_register_external_fabric_CB( &spindle_external_fabric_fe_CB, (void *) &spindle_external_fabric_data);

  /* Get the process table */
  unsigned int ptable_size, actual_size;
  rc = LMON_fe_getProctableSize(aSession, &ptable_size);
  if (rc != LMON_OK) {
     fprintf(stderr, "LMON_fe_getProctableSize failed\n");
     return EXIT_FAILURE;
  }
  MPIR_PROCDESC_EXT *proctab = (MPIR_PROCDESC_EXT *) malloc(sizeof(MPIR_PROCDESC_EXT) * ptable_size);
  rc = LMON_fe_getProctable(aSession, proctab, &actual_size, ptable_size);
  if (rc != LMON_OK) {
     fprintf(stderr, "LMON_fe_getProctable failed\n");
     return EXIT_FAILURE;
  }

  /* Transform the process table to a list of hosts.  Pass through std::set to make hostnames unique */
  set<string> allHostnames;
  transform(proctab, proctab+ptable_size, inserter(allHostnames, allHostnames.begin()), pt_to_string);
  size_t hosts_size = allHostnames.size();
  const char **hosts = (const char **) malloc(hosts_size * sizeof(char *));
  transform(allHostnames.begin(), allHostnames.end(), hosts, string_to_cstr);
  free(proctab);

  /* SPINDLE FE start */
  ldcs_audit_server_fe_md_open( const_cast<char **>(hosts), hosts_size, &md_data_ptr );

  waitfor_done();

  /* Close the server */
  ldcs_audit_server_fe_md_close(md_data_ptr);
  
  rc = LMON_fe_recvUsrDataBe ( aSession, NULL );
  if ( (rc == LMON_EBDARG )
       || ( rc == LMON_ENOMEM )
       || ( rc == LMON_EINVAL ) )  {
      fprintf ( stderr, "[LMON FE] FAILED\n");
      return EXIT_FAILURE;
  }

  sleep (3);

  fprintf ( stdout,
    "\n[LMON FE] PASS: run through the end\n");
  
  return EXIT_SUCCESS;
}

