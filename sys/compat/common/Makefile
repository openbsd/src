#	$NetBSD: Makefile,v 1.3 1995/06/28 03:00:20 cgd Exp $

LIB=	compat
NOPIC=

.PATH:	${COMPATREL}${COMPATDIR}

CC=	${COMPATCC}
CFLAGS+= ${COMPATCFLAGS:S@-I.@-I${COMPATREL}.@g}

SRCS=	compat_tty.c compat_util.c kern_exit_43.c kern_info_09.c \
	kern_info_43.c kern_prot_43.c kern_resource_43.c kern_sig_43.c \
	uipc_syscalls_43.c vfs_syscalls_43.c

# really, all machines were sizeof(int) != sizeof(long)
.if (${MACHINE_ARCH} != "alpha")
SRCS+=	kern_ipc_10.c 
.endif

install:


.include <bsd.lib.mk>
