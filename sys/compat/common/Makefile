#	$OpenBSD: Makefile,v 1.7 1999/05/22 21:22:33 weingart Exp $
#	$NetBSD: Makefile,v 1.8 1996/05/18 15:52:19 christos Exp $

LIB=	compat
NOPIC=

.PATH:	${COMPATDIR}

SRCS=	compat_exec.c compat_util.c compat_vm.c kern_exit_43.c \
	kern_info_09.c kern_info_43.c kern_prot_43.c kern_resource_43.c \
	kern_sig_43.c tty_43.c uipc_syscalls_43.c vfs_syscalls_43.c vm_43.c

# really, all machines were sizeof(int) != sizeof(long)
.if (${MACHINE_ARCH} != "alpha")
SRCS+=	kern_ipc_10.c 
.endif

install:

.include <bsd.lib.mk>
