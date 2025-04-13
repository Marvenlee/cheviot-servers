# Cheviot sysinit server

The root process (pid = 1) of CheviotOS. Loosely based on NetBSD's init.

The IFS filesystem is the first process to start and has the process ID (PID) of 1.
This process forks so that the IFS becomes both the first and the second process. 
At this point the first process replaces itself with sysinit which retains the
first process PID of 1, thereby becoming ,
PID 1 replaces it's image with sysinit so that sysinit becomes the root process.

Runs startup and shutdown scripts. The file /etc/s.startup is the first script
to run which starts core drivers and mounts the root filesystem.

The rc scripts should execute next.

Processes to handle sessions are forked at this point based on entries in
the /etc/ttys file.  At present only the /dev/aux console is used.
Each session starts the getty process.  Currently lgetty is used as a
substitute for getty.  This starts a ksh shell as root without the need
to login.

A message port is created at /servers/sysinit. The message loop accepts
commands from this port to shutdown the OS.  This shall also cleanup after
any terminated sessions or zombie processes.

In response to receiving a shutdown command, the shutdown scripts are
run before sysinit calls into the kernel to halt, restart or power off.

