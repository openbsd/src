#	$OpenBSD: Makefile,v 1.3 2001/08/26 03:23:44 deraadt Exp $
#	$NetBSD: Makefile,v 1.1 1995/10/10 01:19:24 mycroft Exp $

SYS=	../..
DEP=	syscalls.conf syscalls.master ${SYS}/kern/makesyscalls.sh
OBJS=	freebsd_sysent.c freebsd_syscalls.c \
	freebsd_syscall.h freebsd_syscallargs.h

${OBJS}: ${DEP}
	sh ${SYS}/kern/makesyscalls.sh syscalls.conf syscalls.master
