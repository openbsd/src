#	$OpenBSD: Makefile,v 1.6 2016/01/14 16:17:40 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-keyscan
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	ssh-keyscan.1

SRCS=	ssh-keyscan.c

.include <bsd.prog.mk>

LDADD+= -lcrypto -lz
DPADD+= ${LIBCRYPTO} ${LIBZ}
