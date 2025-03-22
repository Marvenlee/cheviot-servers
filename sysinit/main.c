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
  int status;
  struct stat st;
  char *line;
  char *cmd;
  char *buf;
  int sc;
  int pid;
  int startup_cfg_fd;

  dummy_env[0] = NULL;
  environ = dummy_env;
    
  if ((startup_cfg_fd = open("/etc/startup.cfg", O_RDWR)) < 0) {
    log_error("failed to open etc/startup.cfg");
    return -1;
  }

  if ((sc = fstat(startup_cfg_fd, &st)) != 0) {
    log_error("fstat of startup.cfg failed");
    close(startup_cfg_fd);
    return -1;
  }
  
  buf = malloc (st.st_size + 1);
  
  if (buf == NULL) {
    log_error("malloc buffer for startup.cfg script failed");
    close(startup_cfg_fd);
    return -1;
  }
  
  read(startup_cfg_fd, buf, st.st_size);
  buf[st.st_size] = '\0';

  close(startup_cfg_fd);  

  src = buf;   

  while ((line = readLine()) != NULL) {    
    cmd = tokenize(line);
    
    if (cmd == NULL || cmd[0] == '\0') {
      continue;
    }
    
    if (strncmp("#", cmd, 1) == 0 || strlen(cmd) == 0) {
      // Comment in start script, ignore
    } else {
      if (strcmp("start", cmd) == 0) {
        cmdStart(false);
      } else if (strcmp("session", cmd) == 0) {
        cmdStart(true);
      } else if (strcmp("waitfor", cmd) == 0) {
        cmdWaitfor();
      } else if (strcmp("chdir", cmd) == 0) {
        cmdChdir();
      } else if (strcmp("mknod", cmd) == 0) {
        cmdMknod();
      } else if (strcmp("sleep", cmd) == 0) {
        cmdSleep();
      } else if (strcmp("pivot", cmd) == 0) {
        cmdPivot();
      } else if (strcmp("renamemount", cmd) == 0) {
        cmdRenameMount();
      } else if (strcmp("setenv", cmd) == 0) {
        cmdSetEnv();
      } else if (strcmp("settty", cmd) == 0) {
        cmdSettty();
      } else if (strcmp("printgreeting", cmd) == 0) {
        cmdPrintGreeting();
      } else if (strcmp("createport", cmd) == 0) {
        cmdCreatePort();
      } else if (strcmp("handlecommands", cmd) == 0) {
        cmdHandleCommands();
      } else if (strcmp("shutdown", cmd) == 0) {
        cmdShutdown();
      } else {
        log_error("init script, unknown command: %s", cmd);
      }
    }
  }
  
  do {
    pid = waitpid(0, &status, 0);
  } while (sc == 0);
  
  // TODO: Shutdown system/reboot
  while (1) {
    sleep(10);
  }
  
  return 0;
}


/* @brief   Read a line from startup.cfg
 *
 */
char *readLine(void)
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
    
#if 0
    log_info("startup.cfg: %s", linebuf);
#endif
    
    return linebuf;
}


/* @brief   Split a line of text from startup.cfg into nul-terminated tokens
 *
 */
char *tokenize(char *line)
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
int cmdStart (bool session) {
  char *tok;
  int pid;

  for (int t=0; t<ARG_SZ; t++)
  {
    tok = tokenize(NULL);
    argv[t] = tok;
    
    if (tok == NULL) {
      break;
    }
  }

  if (argv[0] == NULL) {
    exit(-1);
  }
    
  pid = fork();
  
  if (pid == 0) {
  
    if (session == true) {
      setsid();
      setpgrp();
      ioctl(STDIN_FILENO, TIOCSCTTY, 0);
    }
    
    execve((const char *)argv[0], argv, NULL);
    exit(-2);
  }
  else if (pid < 0) {
    exit(-1);
  }
  
  return 0;
}


/* @brief   Handle a "mknod" command in startup.cfg
 * 
 * format: mknod <path> <st_mode> <type>
 */
int cmdMknod (void) {
  char *fullpath;
  struct stat st;
  uint32_t mode;
  char *typestr;
  char *modestr;
  int status;

  fullpath = tokenize(NULL);

  if (fullpath == NULL) {
    return -1;
  }
  
  modestr = tokenize(NULL);
  
  if (modestr == NULL) {
    return -1;
  }

  typestr = tokenize(NULL);
  
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
int cmdChdir (void) {
  char *fullpath;
  
  fullpath = tokenize(NULL);

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
int cmdWaitfor (void) {
  char *fullpath;
  int sc;

  struct timespec ts = {
    .tv_sec = 0,
    .tv_nsec = 25000000,
  };

  fullpath = tokenize(NULL);
  
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
int cmdSleep (void) {
    int seconds = 0;
    char *tok;    

    tok = tokenize(NULL);
  
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
int cmdPivot (void) {
  char *new_root;
  char *old_root;
  int sc;
  
  new_root = tokenize(NULL);  
  if (new_root == NULL) {
    return -1;
  }
  
  old_root = tokenize(NULL);
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
int cmdRenameMount(void)
{
  char *new_path;
  char *old_path;
  int sc;
  
  new_path = tokenize(NULL);  
  if (new_path == NULL) {
    return -1;
  }
  
  old_path = tokenize(NULL);
  if (old_path == NULL) {
    return -1;
  }
  
  sc = renamemount(new_path, old_path);
  return sc;
}


/* @brief   Handle a "setty" command in startup.cfg
 *
 * format: settty <tty_device_path>
 */
int cmdSettty(void)
{
  int fd;
  int old_fd;
  char *tty;

  tty = tokenize(NULL);
  
  if (tty == NULL) {
    return -1;
  }
  
  old_fd = open(tty, O_RDWR);  
  
  if (old_fd == -1) {
    return -1;
  }
  
  fd = fcntl(old_fd, F_DUPFD, 20);

  close(old_fd);

  dup2(fd, STDIN_FILENO);
  dup2(fd, STDOUT_FILENO);
  dup2(fd, STDERR_FILENO);
  
  setbuf(stdout, NULL);
    
  tty_set = true;
  return 0;
}


/* @brief   Handle a "setenv" command in startup.cfg
 *
 * format: setenv <name> <value>
 */
int cmdSetEnv(void)
{
  char *name;
  char *value;
  
  name = tokenize(NULL);  
  if (name == NULL) {
    return -1;
  }
  
  value = tokenize(NULL);
  if (value == NULL) {
    return -1;
  }
  
  setenv (name, value, 1);
}


/* @brief   Print a CheviotOS greeting to stdout
 *
 */
void cmdPrintGreeting(void)
{
#if 1
//  printf("\033[0;0H\033[0J\r\n");
  printf("  \033[34;1m   .oooooo.   oooo                               o8o                .   \033[37;1m      .oooooo.    .ooooooo.  \n");
  printf("  \033[34;1m  d8P'  `Y8b  `888                               `^'              .o8   \033[37;1m     d8P'  `Y8b  d8P'   `Y8b \n");
  printf("  \033[34;1m 888           888 .oo.    .ooooo.  oooo    ooo oooo   .ooooo.  .o888oo \033[37;1m    888      888 Y88bo.      \n");
  printf("  \033[34;1m 888           888P^Y88b  d88' `88b  `88.  .8'  `888  d88' `88b   888   \033[37;1m    888      888  `^Y8888o.  \n");
  printf("  \033[34;1m 888           888   888  888ooo888   `88..8'    888  888   888   888   \033[37;1m    888      888      `^Y88b \n");
  printf("  \033[34;1m `88b    ooo   888   888  888    .o    `888'     888  888   888   888 . \033[37;1m    `88b    d88' oo     .d8P \n");
  printf("  \033[34;1m  `Y8bood8P'  o888o o888o `Y8bod8P'     `8'     o888o `Y8bod8P'   `888' \033[37;1m     `Y8bood8P'  `^888888P'  \n");
  printf("\n");
  printf("\033[0m\n\n");
#endif
}


/* @brief  
 */
int cmdCreatePort(void)
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
    exit(-1);
  }

  kq = kqueue();
  
  if (kq < 0) {
    exit(-1);  
  }  

  return 0;  
}


/* @brief
 */
int cmdHandleCommands(void)
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

    if (nevents == 1 && ev.ident == portid && ev.filter == EVFILT_MSGPORT) {
      while ((sc = getmsg(portid, &msgid, &req, sizeof req)) == sizeof req) {
        switch (req.cmd) {
          case CMD_SENDMSG:
            if (req.args.sendmsg.subclass == MSG_SUBCLASS_SYSINIT) {
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

  return 0;
}


/*
 *
 */
int cmdShutdown(void)
{
  shutdown_os(shutdown_how);
  while(1);
}




