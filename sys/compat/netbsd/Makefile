#	$OpenBSD: Makefile,v 1.2 2001/08/26 03:23:44 deraadt Exp $

SYS=	../..
DEP=	syscalls.conf syscalls.master ${SYS}/kern/makesyscalls.sh
OBJS=	netbsd_sysent.c netbsd_syscalls.c \
	netbsd_syscall.h netbsd_syscallargs.h

${OBJS}: ${DEP}
	sh ${SYS}/kern/makesyscalls.sh syscalls.conf syscalls.master
