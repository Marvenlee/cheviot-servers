serversdir = $(prefix)/system/servers
servers_PROGRAMS = sysinit

sysinit_SOURCES = \
  globals.c \
  main.c

sysinit_LDADD = -lsysinit

AM_CFLAGS = -O0 -std=c99 -g2 -march=armv8-a -Wall

