#	$OpenBSD: Makefile,v 1.4 1996/03/03 05:26:21 mickey Exp $
#	$NetBSD: Makefile,v 1.7 1996/02/05 01:58:28 christos Exp $

LIB=	compat
NOPIC=

CPPFLAGS=	${COMPATCPPFLAGS:S/-I./-I${COMPATREL}./g}

.PATH:	${COMPATREL}${COMPATDIR}

SRCS=	compat_util.c kern_exit_43.c kern_info_09.c \
	kern_info_43.c kern_prot_43.c kern_resource_43.c kern_sig_43.c \
	tty_43.c uipc_syscalls_43.c vfs_syscalls_43.c vm_43.c

# really, all machines were sizeof(int) != sizeof(long)
.if (${MACHINE_ARCH} != "alpha")
SRCS+=	kern_ipc_10.c 
.endif

install:

.include <bsd.lib.mk>
