#if !defined(parse_launcher_h_)
#define parse_launcher_h_

/* Error returns for createNewCmdLine */
#define NO_LAUNCHER -1
#define NO_EXEC -2

/* Bitmask of values for the test_launchers parameter */
#define TEST_PRESETUP 1<<0
#define TEST_SLURM    1<<1

int createNewCmdLine(int argc, char *argv[],
                     int *new_argc, char **new_argv[],
                     char *bootstrapper_name,
                     unsigned int test_launchers);

#endif
