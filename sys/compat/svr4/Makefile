#	$OpenBSD: Makefile,v 1.3 2001/08/26 03:23:44 deraadt Exp $
#	$NetBSD: Makefile,v 1.1 1995/10/14 20:24:14 christos Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	svr4_sysent.c svr4_syscalls.c svr4_syscall.h svr4_syscallargs.h

${OBJS}: ${DEP}
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
