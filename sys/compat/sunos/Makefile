#	$OpenBSD: Makefile,v 1.4 2001/08/26 03:23:44 deraadt Exp $
#	$NetBSD: Makefile,v 1.1 1996/05/05 16:07:42 veego Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	sunos_sysent.c sunos_syscalls.c sunos_syscall.h sunos_syscallargs.h

${OBJS}: ${DEP}
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
