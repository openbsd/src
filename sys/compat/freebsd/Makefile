#	$NetBSD: Makefile,v 1.1 1995/10/10 01:19:24 mycroft Exp $

SYS=	../..
DEP=	syscalls.conf syscalls.master ${SYS}/kern/makesyscalls.sh
OBJS=	freebsd_sysent.c freebsd_syscalls.c \
	freebsd_syscall.h freebsd_syscallargs.h

${OBJS}: ${DEP}
	-mv -f freebsd_sysent.c freebsd_sysent.c.bak
	-mv -f freebsd_syscalls.c freebsd_syscalls.c.bak
	-mv -f freebsd_syscall.h freebsd_syscall.h.bak
	-mv -f freebsd_syscallargs.h freebsd_syscallargs.h.bak
	sh ${SYS}/kern/makesyscalls.sh syscalls.conf syscalls.master
