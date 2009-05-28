#	$OpenBSD: Makefile,v 1.6 2009/05/28 16:50:16 andreas Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-keysign
BINOWN=	root

BINMODE?=4555

BINDIR=	/usr/libexec
MAN=	ssh-keysign.8

SRCS=	ssh-keysign.c readconf.c roaming_dummy.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lz
DPADD+=	${LIBCRYPTO} ${LIBZ}
