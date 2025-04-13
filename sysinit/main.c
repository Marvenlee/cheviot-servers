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


/* @brief   The "/serv/sysinit" service
 *
 * @param   argc, unused
 * @param   argv, unused
 * @return  0 on success, non-zero on failure
 *
 * This process is embedded on the initial file system (IFS) along with the
 * kernel and a few other tools. It's purpose is to start any additional
 * servers and device drivers during the inital phase of system startup.
 * 
 * Once other servers that handle the on-disk root filesystem are initialized
 * then this performs a pivot-root, where the root "/" path is pivotted
 * from pointing to the IFS image's root to that of on-disk root filesystem.
 *
 * The IFS executable is the first process started by the kernel. The IFS forks
 * so that the child process becomes the IFS handler server and the parent/root
 * process performs an exec of "/serv/sysinit" to become this process.
 */
int main(int argc, char **argv)
{
  int state;
  
  init_sysinit(argc, argv);
    
  state = STATE_RUN_S_STARTUP;
  
  while(1) {
    switch(state) {
      case STATE_RUN_S_STARTUP:
        state = state_run_s_startup();
        break;

      case STATE_RUN_RC_STARTUP:
        //state = state_run_rc_startup();
        state = STATE_START_SESSIONS; // FIXME
        break;

      case STATE_START_SESSIONS:
        state = state_start_sessions();
        break;

      case STATE_MESSAGE_LOOP:
        state = state_message_loop();
        break;

      case STATE_STOP_SESSIONS:
        state = state_stop_sessions();
        break;

      case STATE_RUN_RC_SHUTDOWN:
        //state = state_run_rc_shutdown();
        state = STATE_RUN_S_SHUTDOWN; // FIXME
        break;

      case STATE_RUN_S_SHUTDOWN:
        state = state_run_s_shutdown();
        break;

      case STATE_REAPER:
        state = state_reaper();
        break;

      case STATE_SHUTDOWN:
        state_shutdown();
        break;
        
      case STATE_FAILURE:
        state = state_failure();
        break;

      default:
        state = STATE_FAILURE;
        break;    
    }
  }
}


