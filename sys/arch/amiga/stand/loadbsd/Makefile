#	$OpenBSD: Makefile,v 1.1 1997/01/16 09:27:00 niklas Exp $

CC = gcc
CFLAGS = -m68030 -O2 -D__progname=program_name -noixemul

OBJS = loadbsd.o getopt.o
LIBS = -lamiga

loadbsd: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o loadbsd $(OBJS) $(LIBS)

clean:
	delete $(OBJS) loadbsd
