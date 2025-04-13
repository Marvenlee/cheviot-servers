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
 *
 */ 
init_state_t state_run_s_startup(void)
{
  if (load_bootstrap_file(PATH_S_STARTUP, &s_startup_script, &s_startup_script_size) != 0) {
    log_error("Failed to load bootstrap file");
    return STATE_FAILURE;
  }
  
  if (load_bootstrap_file(PATH_S_SHUTDOWN, &s_shutdown_script, &s_shutdown_script_size) != 0) {
    log_error("Failed to load bootstrap file");
    return STATE_FAILURE;
  }
  
  if (process_bootstrap_script(s_startup_script) != 0) {
    log_error("Failed to process bootstrap script");
    return STATE_FAILURE;
  }

  return STATE_RUN_RC_STARTUP;  
}


/*
 *
 */
init_state_t state_run_s_shutdown(void)
{
  if (process_bootstrap_script(s_shutdown_script) != 0) {
    log_error("Failed to process bootstrap script");
    return STATE_FAILURE;
  }

  return STATE_REAPER;  
}


/*
 *
 */
int load_bootstrap_file(const char *path, char **r_buf, size_t **r_buf_sz)
{
  int sc;
  int fd;
  char *buf;
  struct stat st;
            
  if ((fd = open(path, O_RDONLY)) < 0) {
    log_error("failed to open %s", path);
    return -1;
  }

  if ((sc = fstat(fd, &st)) != 0) {
    log_error("fstat %s failed", path);
    close(fd);
    return -1;
  }
  
  buf = malloc (st.st_size + 1);
  
  if (buf == NULL) {
    log_error("malloc buffer for %s script failed", path);
    close(fd);
    return -1;
  }
  
  read(fd, buf, st.st_size);
  buf[st.st_size] = '\0';

  close(fd);  

  *r_buf = buf;
  *r_buf_sz = st.st_size;

  return 0;
}


/*
 *
 */
int process_bootstrap_script(char *buf)
{
  char *line;
  char *cmd;
  
  if (buf == NULL) {
    log_error("process_bootstrap_script no buffer");
    return -1;
  }

  // Can we use something like gettyent to get a line?

  src = buf;    // TODO: Use struct or state, pass into readline.
  
  while ((line = s_readline()) != NULL) {    
    cmd = s_tokenize(line);
    
    if (cmd == NULL || cmd[0] == '\0') {
      continue;
    }
    
    if (strncmp("#", cmd, 1) == 0 || strlen(cmd) == 0) {
      // Comment in start script, ignore
    } else {
      if (strcmp("start", cmd) == 0) {
        s_cmd_start(false);
      } else if (strcmp("waitfor", cmd) == 0) {
        s_cmd_waitfor();
      } else if (strcmp("mknod", cmd) == 0) {
        s_cmd_mknod();
      } else if (strcmp("sleep", cmd) == 0) {
        s_cmd_sleep();
      } else if (strcmp("chdir", cmd) == 0) {
        s_cmd_chdir();
      } else if (strcmp("pivot", cmd) == 0) {
        s_cmd_pivot();
      } else if (strcmp("renamemount", cmd) == 0) {
        s_cmd_renamemount();
      } else if (strcmp("createport", cmd) == 0) {
        s_cmd_createport();
      } else {
        log_error("init script, unknown command: %s", cmd);
      }
    }
  }
  
  return 0;
}


/* @brief   Read a line from startup.cfg
 *
 */
char *s_readline(void)
{
    char *dst = linebuf;
    linebuf[255] = '\0';
    
    if (*src == '\0') {
        return NULL;
    }
        
    while (*src != '\0' && *src != '\r' && *src != '\n') {
        *dst++ = *src++;
    }

    if (*src != '\0') {
        src++;
    }
    
    *dst = '\0';
    
    return linebuf;
}


/* @brief   Split a line of text from startup.cfg into nul-terminated tokens
 *
 */
char *s_tokenize(char *line)
{
  static char *ch;
  char separator;
  char *start;
  
  if (line != NULL) {
    ch = line;
  }
  
  while (*ch != '\0') {
    if (*ch != ' ') {
      break;
    }
    ch++;        
  }
  
  if (*ch == '\0') {
    return NULL;
  }        
  
  if (*ch == '\"') {
    separator = '\"';
    ch++;
  } else {
    separator = ' ';
  }

  start = ch;

  while (*ch != '\0') {
    if (*ch == separator) {
      *ch = '\0';
      ch++;
      break;
    }           
    ch++;
  }
          
  return start;    
}


/* @brief   Handle a "start" command in startup.cfg
 *
 * format: start <exe_name> [optional args ...]
 */
int s_cmd_start(bool session)
{
  char *tok;
  int pid;

  for (int t=0; t<ARG_SZ; t++) {
    tok = s_tokenize(NULL);
    start_argv[t] = tok;
    
    if (tok == NULL) {
      break;
    }
  }

  if (start_argv[0] == NULL) {
    return -1;
  }
    
  pid = fork();
  
  if (pid == 0) {  
    if (session == true) {
      setsid();
      setpgrp();
      ioctl(STDIN_FILENO, TIOCSCTTY, 0);
    }
    
    execve((const char *)start_argv[0], start_argv, NULL);
    exit(-2);
  
  } else if (pid < 0) {
    return -1;
  }
  
  return 0;
}


/* @brief   Handle a "mknod" command in startup.cfg
 * 
 * format: mknod <path> <st_mode> <type>
 */
int s_cmd_mknod(void)
{
  char *fullpath;
  struct stat st;
  uint32_t mode;
  char *typestr;
  char *modestr;
  int status;

  fullpath = s_tokenize(NULL);

  if (fullpath == NULL) {
    return -1;
  }
  
  modestr = s_tokenize(NULL);
  
  if (modestr == NULL) {
    return -1;
  }

  typestr = s_tokenize(NULL);
  
  if (typestr == NULL) {
    return -1;
  }
  
  mode = atoi(modestr);
  
  if (typestr[0] == 'c') {
    mode |= _IFCHR;    
  } else if (typestr[0] == 'b') {
    mode |= _IFBLK;
  } else {
    return -1;
  }
  
  st.st_size = 0;
  st.st_uid = 0;
  st.st_gid = 0;
  st.st_mode = mode;
  
  status = mknod2(fullpath, 0, &st);

  return status;
}


/* @brief   Handle a "chdir" command in startup.cfg
 *
 * format: chdir <pathname>
 */
int s_cmd_chdir(void)
{
  char *fullpath;
  
  fullpath = s_tokenize(NULL);

  if (fullpath == NULL) {
    return -1;
  }

  return chdir(fullpath);
}


/* @brief   Handle a "waitfor" command in startup.cfg
 *
 * format: waitfor <path>
 *
 * Waits for a filesystem or device to be mounted at <path>
 *
 * TODO: Add option to timeout
 */
int s_cmd_waitfor(void)
{
  char *fullpath;
  int sc;

  struct timespec ts = {
    .tv_sec = 0,
    .tv_nsec = 25000000,
  };

  fullpath = s_tokenize(NULL);
  
  if (fullpath == NULL) {
    exit(-1);
  }

    
  while (1) {
    if (ismount(fullpath) == 1) {
      break;
    }

    nanosleep(&ts, NULL);
  }

  return 0;  
}


/* @brief   Handle a "sleep" command in startup.cfg
 *
 * format: sleep <seconds>
 */
int s_cmd_sleep(void)
{
  int seconds = 0;
  char *tok;    

  tok = s_tokenize(NULL);

  if (tok == NULL) {
    return -1;
  }

  seconds = atoi(tok);
  sleep(seconds);    
  return 0;
}


/* @brief   Handle a "pivot" command in startup.cfg
 *
 * format: pivot <new_root_path> <old_root_path>
 */
int s_cmd_pivot (void)
{
  char *new_root;
  char *old_root;
  int sc;
  
  new_root = s_tokenize(NULL);  
  if (new_root == NULL) {
    return -1;
  }
  
  old_root = s_tokenize(NULL);
  if (old_root == NULL) {
    return -1;
  }
  
  sc = pivotroot (new_root, old_root);
  return sc;
}


/* @brief   Handle a "remount" command in startup.cfg
 *
 * format: remount <new_path> <old_path>
 */
int s_cmd_renamemount(void)
{
  char *new_path;
  char *old_path;
  int sc;
  
  new_path = s_tokenize(NULL);  
  if (new_path == NULL) {
    return -1;
  }
  
  old_path = s_tokenize(NULL);
  if (old_path == NULL) {
    return -1;
  }
  
  sc = renamemount(new_path, old_path);
  return sc;
}




/* @brief   Handle a "setenv" command in startup.cfg
 *
 * format: setenv <name> <value>
 */
int s_cmd_setenv(void)
{
  char *name;
  char *value;
  
  name = s_tokenize(NULL);  
  if (name == NULL) {
    return -1;
  }
  
  value = s_tokenize(NULL);
  if (value == NULL) {
    return -1;
  }
  
  setenv (name, value, 1);
  return 0;
}


/* @brief  
 */
int s_cmd_createport(void)
{
  int sc;
  struct stat mnt_stat;

  mnt_stat.st_dev = 0;
  mnt_stat.st_ino = 0;
  mnt_stat.st_mode = 0777 | _IFCHR;
  mnt_stat.st_uid = 0;
  mnt_stat.st_gid = 0;
  mnt_stat.st_blksize = 0;
  mnt_stat.st_size = 0;
  mnt_stat.st_blocks = 0;

  portid = createmsgport(SYSINIT_PATH, 0, &mnt_stat);
  
  if (portid < 0) {
    return -1;
  }

  kq = kqueue();
  
  if (kq < 0) {
    return -1;  
  }  

  return 0;  
}



