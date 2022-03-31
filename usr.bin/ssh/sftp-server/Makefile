#	$OpenBSD: Makefile,v 1.14 2022/03/31 03:05:49 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	sftp-server.c sftp-common.c sftp-server-main.c sftp-realpath.c
SRCS+=	atomicio.c fatal.c
SRCS+=	${SRCS_BASE} ${SRCS_UTL}

PROG=	sftp-server

BINDIR=	/usr/libexec
MAN=	sftp-server.8

LDADD+=	-lutil
DPADD+= ${LIBUTIL}

.include <bsd.prog.mk>
