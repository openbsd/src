#	$NetBSD: Makefile,v 1.1 1995/10/14 20:24:14 christos Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	svr4_sysent.c svr4_syscalls.c svr4_syscall.h svr4_syscallargs.h

${OBJS}: ${DEP}
	-mv -f svr4_sysent.c svr4_sysent.c.bak
	-mv -f svr4_syscalls.c svr4_syscalls.c.bak
	-mv -f svr4_syscall.h svr4_syscall.h.bak
	-mv -f svr4_syscallargs.h svr4_syscallargs.h.bak
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
