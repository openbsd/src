#	$NetBSD: Makefile,v 1.1 1995/02/28 23:26:21 fvdl Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	linux_sysent.c linux_syscalls.c linux_syscall.h linux_syscallargs.h

${OBJS}: ${DEP}
	-mv -f linux_sysent.c linux_sysent.c.bak
	-mv -f linux_syscalls.c linux_syscalls.c.bak
	-mv -f linux_syscall.h linux_syscall.h.bak
	-mv -f linux_syscallargs.h linux_syscallargs.h.bak
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
