#	$NetBSD: Makefile,v 1.1 1996/05/05 16:07:42 veego Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	sunos_sysent.c sunos_syscalls.c sunos_syscall.h sunos_syscallargs.h

${OBJS}: ${DEP}
	-mv -f sunos_sysent.c sunos_sysent.c.bak
	-mv -f sunos_syscalls.c sunos_syscalls.c.bak
	-mv -f sunos_syscall.h sunos_syscall.h.bak
	-mv -f sunos_syscallargs.h sunos_syscallargs.h.bak
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
#	$OpenBSD: Makefile,v 1.2 1996/05/10 12:50:22 deraadt Exp $

DEP=	syscalls.conf syscalls.master ../../kern/makesyscalls.sh
OBJS=	sunos_sysent.c sunos_syscalls.c sunos_syscall.h sunos_syscallargs.h

${OBJS}: ${DEP}
	-mv -f sunos_sysent.c sunos_sysent.c.bak
	-mv -f sunos_syscalls.c sunos_syscalls.c.bak
	-mv -f sunos_syscall.h sunos_syscall.h.bak
	-mv -f sunos_syscallargs.h sunos_syscallargs.h.bak
	sh ../../kern/makesyscalls.sh syscalls.conf syscalls.master
