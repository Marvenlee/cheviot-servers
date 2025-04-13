serversdir = $(prefix)/system/servers
servers_PROGRAMS = sysinit

sysinit_SOURCES = \
  globals.c \
  init.c \
  main.c \
  message_loop.c \
  run_rc.c \
  run_s.c \
  sessions.c \
  shutdown.c

sysinit_LDADD = -lsysinit

AM_CFLAGS = -O0 -std=c99 -g2 -march=armv8-a -Wall

