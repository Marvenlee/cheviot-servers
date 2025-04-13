/*
 * Copyright 2014  Marven Gilhespie
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

#ifndef SYSINIT_H
#define SYSINIT_H

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <paths.h>
#include <ttyent.h>


// Constants
#define ARG_SZ 256


// Paths
#define PATH_S_STARTUP    "/etc/s.startup"
#define PATH_S_SHUTDOWN   "/etc/s.shutdown"

#define	PATH_RC_STARTUP   "/etc/rc.startup"
#define PATH_RC_SHUTDOWN	"/etc/rc.shutdown"

#define PATH_TTYTAB       "/etc/ttytab"

#define PATH_SHELL        "/bin/ksh"
#define PATH_CONSOLE      "/dev/aux"     // TODO: Rename 'aux' to 'console'


// Separate state for FSCK or do that in BOOTSTRAP script ?

typedef enum
{
  STATE_RUN_S_STARTUP = 0,
  STATE_RUN_RC_STARTUP,
  STATE_START_SESSIONS,
  STATE_MESSAGE_LOOP,
  STATE_STOP_SESSIONS,
  STATE_RUN_RC_SHUTDOWN,
  STATE_RUN_S_SHUTDOWN,
  STATE_REAPER,
  STATE_SHUTDOWN,
  STATE_FAILURE,
} init_state_t;


// List types
LIST_TYPE(session, session_list_t, session_link_t);


/*
 *
 */
typedef struct session
{
  session_link_t link;
	int	se_index;
	pid_t	se_process;
	struct timeval se_started;
	int	se_flags;
	char *se_device;
	char *se_getty;
	char **se_getty_argv;
} session_t;


#define	SE_SHUTDOWN	(1<<0)	/* session won't be restarted */
#define SE_PRESENT  (1<<1)


/*
 * Prototypes
 */

// init.c
void init_sysinit(int argc, char **argv);

// main.c
int main(int argc, char **argv);

// message_loop.c
init_state_t state_message_loop(void);

// run_rc.c
init_state_t state_run_rc_startup(void);
init_state_t state_run_rc_shutdown(void);
int do_run_rc(const char *path_rc_script);

// run_s.c
init_state_t state_run_s_startup(void);
init_state_t state_run_s_shutdown(void);
int load_s_file(const char *path, char **r_buf, size_t **r_buf_sz);
int process_s_script(char *buf);
char *s_readline(void);
char *s_tokenize(char *line);
int s_cmd_start(bool session);
int s_cmd_chdir(void);
int s_cmd_mknod(void);
int s_cmd_waitfor(void);
int s_cmd_sleep(void);
int s_cmd_settty(void);
int s_cmd_pivot(void);
int s_cmd_renamemount(void);
int s_cmd_setenv(void);

// sessions.c
init_state_t state_start_sessions(void);
init_state_t state_stop_sessions(void);
void collect_process(pid_t pid, int status);
pid_t start_getty(session_t *sp);
void setctty(const char *name);
session_t *find_session(pid_t pid);
session_t *alloc_session(session_t *sprev, int session_index, struct ttyent *typ);
void free_session(session_t *sp);
int setupargv(session_t *sp, struct ttyent *typ);
char **construct_argv(char *command);
int do_setttyent(void);

// shutdown.c
init_state_t state_reaper(void);
void state_shutdown(void);
init_state_t state_failure(void);
void state_shutdown(void);



#endif
