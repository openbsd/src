#	$OpenBSD: Makefile,v 1.15 2026/02/06 23:31:29 dtucker Exp $

.PATH:		${.CURDIR}/..

SRCS=	sftp-server.c sftp-common.c sftp-server-main.c sftp-realpath.c
SRCS+=	atomicio.c fatal.c
SRCS+=	ssherr-nolibcrypto.c
SRCS+=	${SRCS_BASE} ${SRCS_UTL}

PROG=	sftp-server

BINDIR=	/usr/libexec
MAN=	sftp-server.8

LDADD+=	-lutil
DPADD+= ${LIBUTIL}

.include <bsd.prog.mk>
