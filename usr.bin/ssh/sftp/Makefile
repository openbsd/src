#	$OpenBSD: Makefile,v 1.9 2004/11/05 12:19:56 djm Exp $

.PATH:		${.CURDIR}/..

PROG=	sftp
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	sftp.1

SRCS=	sftp.c sftp-client.c sftp-common.c sftp-glob.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto -ledit -ltermcap
DPADD+=	${LIBCRYPTO}
