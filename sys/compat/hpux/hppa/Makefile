#	$OpenBSD: Makefile,v 1.1 2004/07/09 21:48:21 mickey Exp $

DEP=	syscalls.conf syscalls.master ../../../kern/makesyscalls.sh
OBJS=	hpux_sysent.c hpux_syscalls.c hpux_syscall.h hpux_syscallargs.h

${OBJS}: ${DEP}
	sh ../../../kern/makesyscalls.sh syscalls.conf syscalls.master
