/*
 * Copyright 2019  Marven Gilhespie
 *
 * Licensed under the Apache License, segment_id 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_LEVEL_INFO

#include <assert.h>
#include <dirent.h>
#include <errno.h>
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
#include <sys/param.h>
#include <sys/ioctl.h>
#include <ttyent.h>
#include <util.h>
#include "sysinit.h"
#include "sys/sysinit.h"
#include "globals.h"


/*
 * Walk the list of ttys and create sessions for each active line.
 */
init_state_t state_start_sessions(void)
{
  pid_t pid;
	int session_index = 0;
  int started_session_cnt = 0;
	session_t *sp;
	struct ttyent *typ;
  
	do_setttyent();
	
	while ((typ = getttyent()) != NULL) {
		alloc_session(sp, session_index, typ);
    session_index++;
	}
	
	endttyent();

  sp = LIST_HEAD(&session_list);
  
  if (sp == NULL) {
    log_error("no sessions found in /etc/ttys");
    return STATE_RUN_RC_SHUTDOWN;
  }
    
	while(sp != NULL) {
		if (sp->se_process) {
			continue;
    }
    
		if ((pid = start_getty(sp)) == -1) {
		  log_error("Unable to start a session");
		  
		  if (started_session_cnt == 0) {
		    return STATE_RUN_RC_SHUTDOWN;
		  } else {
		  	break;
		  }
		}
		
		sp->se_process = pid;	
		gettimeofday(&sp->se_started, NULL);
	
		sp = LIST_NEXT(sp, link);
	}

	return STATE_MESSAGE_LOOP;
}


/*
 *
 */
init_state_t state_stop_sessions(void)
{
  return STATE_RUN_RC_SHUTDOWN;
}



/*
 * Collect exit status for a child process.
 * If an exiting login, start a new login running.
 */
void collect_process(pid_t pid, int status)
{
	session_t *sp;

  log_info("collect_process(pid:%d, status:%d)", pid, status);

	if (LIST_HEAD(&session_list) == NULL) {
		return 0;
  }

	if ((sp = find_session(pid)) == NULL) {
		return 0;
  }
  
	if (sp->se_flags & SE_SHUTDOWN) {
		free_session(sp);
		return 0;
	}

  // Restart the session and reinitialize fields
	if ((pid = start_getty(sp)) == -1) {
    log_error("failed to restart session");
		/* serious trouble */
		return -1;
	}
	
	sp->se_process = pid;
	gettimeofday(&sp->se_started, NULL);	
	
	return 0;
}


/*
 * Start a login session running.
 */
pid_t start_getty(session_t *sp)
{
	pid_t pid;
	sigset_t mask;
	time_t current_time = time(NULL);

	if ((pid = fork()) == -1) {
		log_error("can't fork for getty on port: %s", sp->se_device);
		return -1;
	}

	if (pid > 0) {
		return pid;
  }

	sigemptyset(&mask);
	sigprocmask(SIG_SETMASK, &mask, (sigset_t *) 0);

  setctty(sp->se_device);
  
  execve((const char *)sp->se_getty_argv[0], sp->se_getty_argv, NULL);

	_exit(8);
	/*NOTREACHED*/
	return -1;
}



/*
 * Used for setting controlling terminal of rc session.
 */
void setctty(const char *name)
{
	int fd;
  int old_fd;
  
	if (setsid() < 0) {
		log_warn("setctty setsid() failed");
	}
	
	if ((fd = open(name, O_RDWR)) == -1) {
		log_error("can't open %s: %m", name);
		_exit(1);
	}

  old_fd = fd;

  fd = fcntl(old_fd, F_DUPFD, 20);
  
  close(old_fd);
	
	if (fd == -1) {
    log_error("Unable to dup fd to higher fd");
	 _exit(1);
	}
		
	if (login_tty(fd) == -1) {
		log_error("can't get %s for controlling terminal: %m", name);
		_exit(2);
	}
}


/*
 *
 */
int login_tty(int fd)
{
  int in_fd, out_fd, err_fd;
  pid_t sid, pgrp;
  
	sid = setsid();
  pgrp = setpgrp();

	if (ioctl(fd, TIOCSCTTY, NULL) == -1) {
	  log_error("ioctl TIOCSCTTY failed");
		return (-1);
  }
  		
	in_fd = dup2(fd, STDIN_FILENO);
	out_fd = dup2(fd, STDOUT_FILENO);
	err_fd = dup2(fd, STDERR_FILENO);
	
	if (in_fd != STDIN_FILENO || out_fd != STDOUT_FILENO || err_fd != STDERR_FILENO) {
		log_error("failed to dup stdin, stdout, stderr, %d, %d, %d", in_fd, out_fd, err_fd);
		close(fd);
  }
  	
	return (0);
}


/*
 *
 */
session_t *find_session(pid_t pid)
{
  session_t *sp;
  
  sp = LIST_HEAD(&session_list);
  
  while (sp != NULL) {  
    if (sp->se_process == pid) {
      return sp;
    }
  
    sp = LIST_NEXT(sp, link);
  }
  
  return NULL;
}


/*
 * Allocate a new session descriptor.
 */
session_t *alloc_session(session_t *sprev, int session_index, struct ttyent *typ)
{
	session_t *sp;

	if ((typ->ty_status & TTY_ON) == 0 || typ->ty_name == NULL || typ->ty_getty == NULL) {
    log_error("alloc_session failed, status, name or getty string are null");
		return NULL;
  }
  
	sp = malloc(sizeof (session_t));

	if (sp == NULL) {
	  log_error("failed to allocate session struct");
		return NULL;
	}

	memset(sp, 0, sizeof *sp);

	sp->se_flags = SE_PRESENT;
	sp->se_index = session_index;

	asprintf(&sp->se_device, "%s%s", _PATH_DEV, typ->ty_name);

	if (!sp->se_device) {
	  log_error("session device path null");
		free(sp);
		return NULL;
	}

	if (setupargv(sp, typ) == 0) {
	  log_error("failed to setup argv to pass to getty");
		free_session(sp);
		return NULL;
	}

  LIST_ADD_TAIL(&session_list, sp, link);
  
	return sp;
}


/*
 * Free a session descriptor.
 */
void free_session(session_t *sp)
{
  LIST_REM_ENTRY(&session_list, sp, link);
  
	free(sp->se_device);

	if (sp->se_getty) {
		free(sp->se_getty);
		free(sp->se_getty_argv);
	}

	free(sp);
}


/* @brief   Setup get path and args to pass to getty
 */
int setupargv(session_t *sp, struct ttyent *typ)
{
	if (sp->se_getty) {
		free(sp->se_getty);
		free(sp->se_getty_argv);
	}

	asprintf(&sp->se_getty, "%s %s", typ->ty_getty, typ->ty_name);
	
	if (!sp->se_getty) {
		return 0;
  }
  		
	sp->se_getty_argv = construct_argv(sp->se_getty);
	
	if (sp->se_getty_argv == NULL) {
		log_warn("can't parse getty for port `%s'", sp->se_device);
		free(sp->se_getty);
		sp->se_getty = NULL;
		return 0;
	}

	return 1;
}


/* @brief   Create an argv array from a command line specified in /etc/ttys.
 *
 */
char **construct_argv(char *command)
{
	int argc = 0;
	const char separators[] = " \t";
	char **argv;

  argv = malloc(((strlen(command) + 1) / 2 + 1) * sizeof (char *));

	if (argv == NULL) {
		return NULL;
  }
  
	if ((argv[argc++] = strtok(command, separators)) == 0) {
		free(argv);
		return NULL;
	}

	while ((argv[argc++] = strtok(NULL, separators)) != NULL) {
	}

	return argv;
}


/*
 *
 */
int do_setttyent(void)
{
	endttyent();
  return setttyent();
}




