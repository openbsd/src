#	$OpenBSD: Makefile,v 1.9 2013/07/02 13:32:38 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	sftp-server
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec
MAN=	sftp-server.8

SRCS=	sftp-server.c sftp-common.c sftp-server-main.c

LDADD+=	-lutil
DPADD+= ${LIBUTIL}

.include <bsd.prog.mk>
