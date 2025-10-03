#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
typedef enum {
   set_true,
   set_false,
   unset,
} env_t;

typedef enum {
   e_execl,
   e_execlp,
   e_execle,
   e_execv,
   e_execvp,
   e_execvpe,
   e_execve,
   e_execve_bare,
   e_execvpe_bare,
   e_execle_bare,
} exec_t;

extern char **environ;

static char **setup_envp(env_t envt, exec_t exect)
{
   int explicit_env = 0, bare_env = 0;
   switch (exect) {
      case e_execle:
      case e_execvpe:
      case e_execve:         
         explicit_env = 1;
         bare_env = 0;
         break;
      case e_execl:
      case e_execlp:
      case e_execv:
      case e_execvp:
         explicit_env = 0;
         bare_env = 0;
         break;
      case e_execve_bare:
      case e_execvpe_bare:
      case e_execle_bare:
         explicit_env = 1;
         bare_env = 1;
         break;
   }
   if (explicit_env) {
      int envsize;
      char **newenv;
      for (envsize = 0; environ[envsize] != NULL; envsize++);
      newenv = (char **) malloc((envsize+2) * sizeof(char*));
      int i, j;
      for (i = 0, j = 0; environ[i] != NULL; i++) {
         if (strncmp("SPINDLE=", environ[i], strlen("SPINDLE=")) == 0) {
            switch (envt) {
               case set_true:
                  newenv[j++] = "SPINDLE=true";
                  break;
               case set_false:
                  newenv[j++] = "SPINDLE=false";
                  break;
               case unset:
                  break;
            }
         }
         else if (!bare_env) {
            newenv[j++] = environ[i];
         }
      }
      newenv[j++] = "SPINDLE_EXEC_TEST=correct";
      newenv[j++] = NULL;
      return newenv;
   }
   else { //!explicit_env
      switch (envt) {
         case set_true:
            setenv("SPINDLE", "true", 1);
            break;
         case set_false:
            setenv("SPINDLE", "false", 1);
            break;
         case unset:
            unsetenv("SPINDLE");
            break;
      }
      setenv("SPINDLE_EXEC_TEST", "correct", 1);
      
      return NULL;
   }
}

static char **setup_argv(exec_t exect)
{
   int path_search = 0;
   switch (exect) {
      case e_execlp:
      case e_execvpe:
      case e_execvp:
      case e_execvpe_bare:
         path_search = 1;
         break;
      case e_execle:
      case e_execve:
      case e_execl:
      case e_execv:
      case e_execve_bare:
      case e_execle_bare:
         path_search = 0;
         break;
   }

   char **argv = (char **) malloc(3 * sizeof(char*));
   argv[0] = path_search ? "spindle_deactivated.sh" : "./spindle_deactivated.sh";
   argv[1] = "spindletest";
   argv[2] = NULL;
   return argv;
}

extern char **environ;
static int runproc(env_t envt, exec_t exect)
{
   pid_t pid = fork();
   if (pid == 0) {
      char **envp, **argv;
      envp = setup_envp(envt, exect);
      argv = setup_argv(exect);

      int result;
      switch (exect) {
         case e_execl:
            result = execl(argv[0], argv[0], argv[1], NULL);
            break;
         case e_execlp:
            result = execlp(argv[0], argv[0], argv[1], NULL);
            break;
         case e_execle:
         case e_execle_bare:
            result = execle(argv[0], argv[0], argv[1], NULL, envp);
            break;
         case e_execv:
            result = execv(argv[0], argv);
            break;
         case e_execvp:
            result = execvp(argv[0], argv);
            break;
         case e_execvpe:
         case e_execvpe_bare:
            result = execvpe(argv[0], argv, envp);
            break;
         case e_execve:
         case e_execve_bare:
            result = execve(argv[0], argv, envp);
            break;
      }
      assert(result == -1);
      exit(-1);
   }
   if (pid != 0) {
      int status, result;
      do {
         result = waitpid(pid, &status, 0);
      } while (result != -1 && !WIFEXITED(status) && !WIFSIGNALED(status));
      if (result == -1) {
         int error = errno;
         fprintf(stderr, "Failed on return from waitpid: %s\n", strerror(error));
         return -1;
      }
      if (WIFSIGNALED(status)) {
         fprintf(stderr, "Failed, process exited on signal\n");
         return -1;
      }
      int exitcode = WEXITSTATUS(status);
      if (exitcode == 255 || exitcode == -1) {
         fprintf(stderr, "Failed, process exited -1\n");
         return -1;
      }

      int expected = 0;
      switch (envt) {
        case set_true:
           expected = 0;
           break;
        case set_false:
           expected = 1;
           break;
        case unset:
           expected = 0;
           break;
      }
      if (exitcode != expected) {
         fprintf(stderr, "Failed, spindle state was incorrect. Expected %d, got %d\n", expected, exitcode);
         return -1;
      }
   }

   return 0;
}

#define STR2(X) #X
#define STR(X) STR2(X)

#define STRCASE(X) case X: s = STR(X); break

static int had_error = 0;
static void runtest(env_t envt, exec_t exect)
{
   int result;
   char *s, *env_s, *exec_s;
   result = runproc(envt, exect);
   if (result == 0)
      return;
   had_error = 1;
   switch (envt) {
      STRCASE(set_true);
      STRCASE(set_false);
      STRCASE(unset);
   }
   env_s = s;
   switch (exect) {
      STRCASE(e_execl);
      STRCASE(e_execlp);
      STRCASE(e_execle);
      STRCASE(e_execv);
      STRCASE(e_execvp);
      STRCASE(e_execvpe);
      STRCASE(e_execve);
      STRCASE(e_execve_bare);
      STRCASE(e_execvpe_bare);
      STRCASE(e_execle_bare);
   }
   exec_s = s;

   fprintf(stderr, "ERROR: %s on %s test case\n", env_s, exec_s);
   return;
}

void setuppath()
{
   char *path, *newpath;
   int pathlen;
   path = getenv("PATH");
   pathlen = strlen(path) + 3;
   newpath = (char *) malloc(pathlen);
   snprintf(newpath, pathlen, "%s:.", path);
   setenv("PATH", newpath, 1);
}

int main(int argc, char *argv[])
{
   setuppath();
   runtest(set_true, e_execl);
   runtest(set_true, e_execlp);
   runtest(set_true, e_execle);
   runtest(set_true, e_execv);
   runtest(set_true, e_execvp);
   runtest(set_true, e_execvpe);
   runtest(set_true, e_execve);
   runtest(set_true, e_execve_bare);
   runtest(set_true, e_execvpe_bare);
   runtest(set_true, e_execle_bare);

   runtest(set_false, e_execl);
   runtest(set_false, e_execlp);
   runtest(set_false, e_execle);
   runtest(set_false, e_execv);
   runtest(set_false, e_execvp);
   runtest(set_false, e_execvpe);
   runtest(set_false, e_execve);
   runtest(set_false, e_execve_bare);
   runtest(set_false, e_execvpe_bare);
   runtest(set_false, e_execle_bare);

   runtest(unset, e_execl);
   runtest(unset, e_execlp);
   runtest(unset, e_execle);
   runtest(unset, e_execv);
   runtest(unset, e_execvp);
   runtest(unset, e_execvpe);
   runtest(unset, e_execve);
   runtest(unset, e_execve_bare);
   runtest(unset, e_execvpe_bare);
   runtest(unset, e_execle_bare);

   if (had_error) {
      printf("FAILED\n");
      return -1;
   }
   else {
      printf("PASSED\n");
      return 0;
   }
}
           
