#	$OpenBSD: Makefile,v 1.5 2001/08/26 03:23:44 deraadt Exp $
#	$NetBSD: Makefile,v 1.1 1995/02/13 21:39:00 cgd Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	osf1_sysent.c osf1_syscalls.c osf1_syscall.h osf1_syscallargs.h

${OBJS}: ${DEP}
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master

clean:
	rm -f ${OBJS}
