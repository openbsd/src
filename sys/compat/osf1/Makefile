#	$NetBSD: Makefile,v 1.1 1995/02/13 21:39:00 cgd Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	osf1_sysent.c osf1_syscalls.c osf1_syscall.h osf1_syscallargs.h

${OBJS}: ${DEP}
	-mv -f osf1_sysent.c osf1_sysent.c.bak
	-mv -f osf1_syscalls.c osf1_syscalls.c.bak
	-mv -f osf1_syscall.h osf1_syscall.h.bak
	-mv -f osf1_syscallargs.h osf1_syscallargs.h.bak
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
