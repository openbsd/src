#	$OpenBSD: Makefile,v 1.1 1996/08/02 20:35:50 niklas Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	ultrix_sysent.c ultrix_syscalls.c ultrix_syscall.h ultrix_syscallargs.h

${OBJS}: ${DEP}
	-mv -f ultrix_sysent.c ultrix_sysent.c.bak
	-mv -f ultrix_syscalls.c ultrix_syscalls.c.bak
	-mv -f ultrix_syscall.h ultrix_syscall.h.bak
	-mv -f ultrix_syscallargs.h ultrix_syscallargs.h.bak
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
