#	$OpenBSD: Makefile,v 1.4 2014/09/01 05:20:54 guenther Exp $
#	$NetBSD: Makefile,v 1.1 1995/02/28 23:26:21 fvdl Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	linux_sysent.c linux_syscalls.c linux_syscall.h linux_syscallargs.h

all:
	@echo "Doing nothing.  Did you want make syscalls?"

syscalls:	linux_sysent.c

${OBJS}: ${DEP}
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
