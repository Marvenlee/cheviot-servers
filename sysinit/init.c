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
 * Initialize sysinit.
 *
 * Note: Logging will be to console until we have syslog server running.
 */
void init_sysinit(int argc, char **argv)
{
  dummy_env[0] = NULL;
  environ = dummy_env;

	if (getuid() != 0) {
	  log_error("sysinit run from non-root user");
		exit(EXIT_FAILURE);
	}

	if (getpid() != 1) {
		log_error("sysinit is not root process pid 1");
		exit(EXIT_FAILURE);
  }
  
	// Create an initial session.  FIXME: Needed?

	if (setsid() < 0) {
		log_error("initial setsid() failed");
  }
  
  /* TODO: We may want to set up signal handlers here.
   * Unlike other unices we use messages instead of signals to 
   * request shutdown.  But the /serv/sysinit port is not created
   * until later, so the early drivers have no way of requesting
   * shutdown on panic or failure.
   */

	// Close stdin, stdout, stderr. These shouldn't be open.
	close(stdin);
	close(stdout);
	close(stderr);
}


