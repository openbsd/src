#	$OpenBSD: Makefile,v 1.1 1996/08/02 20:34:52 niklas Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	hpux_sysent.c hpux_syscalls.c hpux_syscall.h hpux_syscallargs.h

${OBJS}: ${DEP}
	-mv -f hpux_sysent.c hpux_sysent.c.bak
	-mv -f hpux_syscalls.c hpux_syscalls.c.bak
	-mv -f hpux_syscall.h hpux_syscall.h.bak
	-mv -f hpux_syscallargs.h hpux_syscallargs.h.bak
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
