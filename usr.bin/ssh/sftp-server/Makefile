#	$OpenBSD: Makefile,v 1.4 2001/02/04 11:11:56 djm Exp $

.PATH:		${.CURDIR}/..

PROG=	sftp-server
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec
MAN=	sftp-server.8

SRCS=	sftp-server.c sftp-common.c log-server.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
