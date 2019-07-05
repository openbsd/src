#	$OpenBSD: Makefile,v 1.13 2019/07/05 04:55:41 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	sftp-server.c sftp-common.c sftp-server-main.c sftp-realpath.c
SRCS+=	fatal.c
SRCS+=	${SRCS_BASE} ${SRCS_UTL}

PROG=	sftp-server

BINDIR=	/usr/libexec
MAN=	sftp-server.8

LDADD+=	-lutil
DPADD+= ${LIBUTIL}

.include <bsd.prog.mk>
