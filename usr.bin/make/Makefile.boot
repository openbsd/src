#	$OpenBSD: Makefile.boot,v 1.2 1996/06/26 05:36:24 deraadt Exp $
#	$NetBSD: Makefile.boot,v 1.5 1995/06/14 15:18:43 christos Exp $
# a very simple makefile...
#
# You only want to use this if you aren't running NetBSD.
#
# modify MACHINE and MACHINE_ARCH as appropriate for your target architecture
#
CFLAGS= -I. -DMACHINE=\"sparc\" -DMACHINE_ARCH=\"sparc\" \
	-DMAKE_BOOTSTRAP

OBJ=arch.o buf.o compat.o cond.o dir.o for.o hash.o job.o main.o make.o \
    parse.o str.o suff.o targ.o var.o util.o

bmake: ${OBJ}
	@echo 'make of make and make.0 started.'
	(cd lst.lib; make)
	${CC} *.o lst.lib/*.o -o bmake
#	nroff -h -man make.1 > make.0
#	@echo 'make of make and make.0 completed.'
