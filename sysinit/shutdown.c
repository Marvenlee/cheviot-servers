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

#define LOG_LEVEL_INFO

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
 * Bring the system down to single user.
 */
init_state_t state_reaper(void)
{
	session_t *sp;
	int status;
	pid_t pid;
	static const int death_sigs[3] = { SIGHUP, SIGTERM, SIGKILL };

  sp = LIST_HEAD(&session_list);

	while (sp != NULL) {
		sp->se_flags |= SE_SHUTDOWN;
    sp = LIST_NEXT(sp, link);
  }
  
	for (int i = 0; i < 3; ++i) {
		if (kill(-1, death_sigs[i]) == -1 && errno == ESRCH) {
			return STATE_SHUTDOWN;
    }
    
		do {
			if ((pid = waitpid(-1, &status, 0)) != -1) {
				collect_process(pid, status);
			}
				
		} while (errno != ECHILD);

    return STATE_SHUTDOWN;
	}

	log_warn("some processes would not die; ps axl advised");

	return STATE_SHUTDOWN;
}


/*
 *
 */
init_state_t state_failure(void)
{
  log_error("state failure");
  return STATE_SHUTDOWN;
}


/*
 *
 */
void state_shutdown(void)
{
  shutdown_os(shutdown_how);
  while(1);
}

