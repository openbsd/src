# Generated automatically from Makefile.in by configure.
# === makefile ============================================================
# Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu
#
#	Description: This file is for creating the test programs for libpthread.a
#
#	1.00 93/08/03 proven
#		-Initial cut for pthreads.
#

srctop = /dr1/my/masters/mysql/mit-pthreads
srcdir = /dr1/my/masters/mysql/mit-pthreads/bin/finger
VPATH = /dr1/my/masters/mysql/mit-pthreads/bin/finger
prefix= /usr/local/pthreads
exec_prefix= ${prefix}

INSTALL_PATH = ${prefix}
      BINDIR = $(INSTALL_PATH)/bin
      LIBDIR = $(INSTALL_PATH)/lib
      MANDIR = $(INSTALL_PATH)/man
      INCDIR = $(INSTALL_PATH)/include

          CC = ../../pgcc -notinstalled
 CDEBUGFLAGS = -g -O2 -Werror
    INCLUDES = -I/dr1/my/masters/mysql/mit-pthreads/lib/libpthreadutil/ -L../../lib/libpthreadutil/
      CFLAGS = $(CDEBUGFLAGS) $(INCLUDES) $(ADDL_CFLAGS) -DSRCDIR=\"$(srcdir)\"
      RANLIB = ranlib

        OBJS = finger.o net.o
      BINARY = finger

################################################################################
#
all : $(BINARY)

clean: 
	rm -f *.o $(TESTS) $(BENCHMARKS) a.out core maketmp makeout

depend:
	sed '/\#\#\# Dependencies/q' < Makefile > maketmp
	(for i in $(CSRC);do $(CPP) -M $$i;done) >> maketmp
	cp maketmp Makefile

install: $(BINARY)
	install $(BINARY) $(BINDIR)

realclean: clean
	rm -f Makefile

Makefile: Makefile.in
	(cd ../.. ; sh config.status)

.c.o:
	$(CC) $(CFLAGS) -c $< 

$(BINARY) : ${OBJS} 
	$(CC) $(CFLAGS) -o $(BINARY) ${OBJS} -lpthreadutil

################################################################################
### Do not remove the following line. It is for depend #########################
### Dependencies:
