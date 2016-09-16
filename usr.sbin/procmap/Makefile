#	$OpenBSD: Makefile,v 1.6 2016/09/16 04:45:35 dlg Exp $

PROG=		procmap
SRCS=		procmap.c
CFLAGS+=	-Wall
LDADD=		-lkvm
DPADD=		${LIBKVM}

.PATH: ${.CURDIR}/../../sys/kern
SRCS+=		subr_tree.c

.include <bsd.prog.mk>
