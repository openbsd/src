#	$OpenBSD: Makefile,v 1.2 1998/03/29 22:24:49 espie Exp $

CC = gcc
CFLAGS = -m68030 -Wall -O3 -fomit-frame-pointer -D__progname=program_name -msmall-code -resident

OBJS = loadbsd.o getopt.o sleep.o vers.o vers_ixemul.o openlibs_ixemul.o openlibs_stub.o
OBJS1 = loadbsd.o getopt.o sleep.o vers.o openlibs_stub.o
OBJS2 = loadbsd.o vers_ixemul.o openlibs_ixemul.o
PROGS=loadbsd loadbsd.ixemul

# add -lamiga if you can't get inlines to work
LIBS = 
LDFLAGS=-s

all: $(PROGS)

loadbsd: $(OBJS1)
	$(CC) $(CFLAGS) $(LDFLAGS) -noixemul -o loadbsd $(OBJS1) $(LIBS)

loadbsd.ixemul: $(OBJS2)
	$(CC) $(CFLAGS) $(LDFLAGS) -o loadbsd.ixemul $(OBJS2) $(LIBS)

clean:
	-rm $(OBJS) $(PROGS)
