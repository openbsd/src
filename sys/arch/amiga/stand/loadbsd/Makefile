#	$OpenBSD: Makefile,v 1.5 2002/12/02 09:00:22 miod Exp $

CC = gcc
CFLAGS = -m68030 -Wall -O3 -fomit-frame-pointer -D__progname=program_name -msmall-code -resident 
CFLAGS+=	-fno-stack-protector

OBJS1 = loadbsd.o poolmem.o getopt.o sleep.o vers.o openlibs_stub.o
OBJS2 = loadbsd.o poolmem.o vers_ixemul.o openlibs_ixemul.o
PROGS=loadbsd loadbsd.ixemul

# add -lamiga if you can't get inlines to work
LIBS = 
LDFLAGS=-s

all: $(PROGS) distrib

loadbsd: $(OBJS1)
	$(CC) $(CFLAGS) $(LDFLAGS) -noixemul -o loadbsd $(OBJS1) $(LIBS)

loadbsd.ixemul: $(OBJS2)
	$(CC) $(CFLAGS) $(LDFLAGS) -o loadbsd.ixemul $(OBJS2) $(LIBS)

distrib: loadbsd
	gzip -c loadbsd >loadbsd.gz
	echo loadbsd built on `date` with gcc `gcc -dumpversion` >loadbsd.gz.uu
	uuencode loadbsd.gz loadbsd.gz >>loadbsd.gz.uu

clean:
	-rm $(OBJS1) $(OBJS2) $(PROGS)
