#	$OpenBSD: Makefile,v 1.15 2004/01/28 01:39:39 mickey Exp $
#	$NetBSD: Makefile,v 1.8 1996/05/18 15:52:19 christos Exp $

LIB=	compat
NOPIC=

MACHINE=	${XMACHINE}
MACHINE_ARCH=	${XMACHINE_ARCH}

.PATH:	${COMPATDIR}

SRCS=	compat_exec.c compat_util.c compat_dir.c compat_vm.c \
	kern_exit_43.c kern_ipc_23.c kern_info_09.c kern_info_43.c \
	kern_resource_43.c kern_sig_43.c tty_43.c uipc_syscalls_43.c \
	vfs_syscalls_25.c vfs_syscalls_43.c vm_43.c

# really, all machines where sizeof(int) != sizeof(long)
.if (${MACHINE_ARCH} != "alpha") && (${MACHINE_ARCH} != "x86_64") && \
    (${MACHINE_ARCH} != "sparc64")
SRCS+=	kern_ipc_10.c 
.endif

install:

.include <bsd.lib.mk>
