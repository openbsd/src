#	$OpenBSD: Makefile,v 1.19 2001/03/03 23:59:37 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-add
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	ssh-add.1

SRCS=	ssh-add.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil -lz
DPADD+= ${LIBCRYPTO} ${LIBDES} ${LIBUTIL} ${LIBZ}
