#	$OpenBSD: Makefile,v 1.1 1997/09/02 08:54:11 downsj Exp $

SYS=	../..
DEP=	syscalls.conf syscalls.master ${SYS}/kern/makesyscalls.sh
OBJS=	bsdos_sysent.c bsdos_syscalls.c \
	bsdos_syscall.h bsdos_syscallargs.h

${OBJS}: ${DEP}
	-mv -f bsdos_sysent.c bsdos_sysent.c.bak
	-mv -f bsdos_syscalls.c bsdos_syscalls.c.bak
	-mv -f bsdos_syscall.h bsdos_syscall.h.bak
	-mv -f bsdos_syscallargs.h bsdos_syscallargs.h.bak
	sh ${SYS}/kern/makesyscalls.sh syscalls.conf syscalls.master
