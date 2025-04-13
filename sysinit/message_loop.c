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
 *
 */
init_state_t state_message_loop(void)
{
  int sc;
  msgid_t msgid;
  iorequest_t req;
  int nevents;
  struct kevent ev;
  struct sysinit_req sysinit_req;
  
  EV_SET(&ev, portid, EVFILT_MSGPORT, EV_ADD | EV_ENABLE, 0, 0, 0); 
  kevent(kq, &ev, 1,  NULL, 0, NULL);

  while (!shutdown) {
    errno = 0;
    nevents = kevent(kq, NULL, 0, &ev, 1, NULL);

    // TODO: Maybe check sigchld, handle with waitpid/reap_processes

    if (nevents == 1 && ev.ident == portid && ev.filter == EVFILT_MSGPORT) {
      while ((sc = getmsg(portid, &msgid, &req, sizeof req)) == sizeof req) {
        switch (req.cmd) {
          case CMD_SENDIO:
            if (req.args.sendio.subclass == MSG_SUBCLASS_SYSINIT) {
            
              // TODO: Determine command
            
              readmsg(portid, msgid, &sysinit_req, sizeof sysinit_req, 0);              
              shutdown_how = sysinit_req.u.shutdown.how;
              shutdown = true;
              replymsg(portid, msgid, 0, NULL, 0);
            } else {
              replymsg(portid, msgid, -ENOTSUP, NULL, 0);
            }
                                              
            break;

          default:
            log_warn("unknown command: %d", req.cmd);
            replymsg(portid, msgid, -ENOTSUP, NULL, 0);
            break;
        }
      }      
    }
  }

  return STATE_STOP_SESSIONS;
}



/* @brief   reap any dead zombie processes
 * 
 * TODO: Register notification with kqueue above
 */
void reap_processes(void)
{
  log_info("reap_processes");
  
  while(waitpid(-1, NULL, 0) != 0) {
    sleep(5);
  }

  // call collect_process() to handle any cleanup and restart sessions.
  
  log_info("reap_processes exiting, waitpid returned 0");
}

