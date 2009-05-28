#	$OpenBSD: Makefile,v 1.5 2009/05/28 16:50:16 andreas Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-keyscan
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	ssh-keyscan.1

SRCS=	ssh-keyscan.c roaming_dummy.c

.include <bsd.prog.mk>

LDADD+= -lcrypto -lz
DPADD+= ${LIBCRYPTO} ${LIBZ}
