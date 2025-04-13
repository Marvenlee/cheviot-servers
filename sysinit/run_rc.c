#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscalls.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/syslimits.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/termios.h>
#include <sys/ioctl.h>
#include "sysinit.h"
#include "sys/sysinit.h"
#include "globals.h"


/*
 *
 */
init_state_t state_run_rc_startup(void)
{
  int sc;
      
  sc = do_run_rc(PATH_RC_STARTUP);
  
  if (sc != 0) {
    return STATE_RUN_RC_SHUTDOWN;
  } else {
    return STATE_START_SESSIONS;
  }
}


/*
 *
 */
init_state_t state_run_rc_shutdown(void)
{
  do_run_rc(PATH_RC_SHUTDOWN);
  
  return STATE_RUN_S_SHUTDOWN;
}


/*
 *
 */
int do_run_rc(const char *path_rc_script)
{
	pid_t pid, wpid;
	int status;
	char *argv[4];
	struct sigaction sa;


  pid = fork();

  if (pid == -1) {
		while (waitpid(-1, NULL, WNOHANG) > 0) {
			continue;
    }
    
		return -1;  

  } else if (pid == 0) {
    // Child process
    
		sigemptyset(&sa.sa_mask);
		
		sa.sa_flags = 0;
		sa.sa_handler = SIG_IGN;
		
		sigaction(SIGTSTP, &sa, NULL);
		sigaction(SIGHUP, &sa, NULL);

		setctty(PATH_CONSOLE);

		argv[0] = PATH_SHELL;
		argv[1] = path_rc_script;
		argv[2] = 0;
		argv[3] = 0;

		sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL);

		execv(PATH_SHELL, (const char *)argv);
		
		log_error("can't exec %s for %s", PATH_SHELL, path_rc_script);

		_exit(5);	/* force single user mode */		
		// Not possible
		
  } else {
    // parent process
    
    do {
      wpid = waitpid(-1, &status, WUNTRACED);
    
	    if (wpid != -1) {
		    collect_process(wpid, status);
		  }
		  
		  if (wpid == -1) {
		    if (errno == EINTR) {
			    continue;
		    }

    		return -1;  
	    }
	    
	    if (wpid == pid && WIFSTOPPED(status)) {
		    kill(pid, SIGCONT);
		    wpid = -1;
	    }
	    
    } while (wpid != pid);

    if (!WIFEXITED(status)) {
  		return -1;  
    }

    if (WEXITSTATUS(status) != 0) {
	    return -1;
    }
    
    return 0;
  }
}


