#	$OpenBSD: Makefile,v 1.11 2018/01/08 15:37:23 markus Exp $

.PATH:		${.CURDIR}/..

SRCS=	sftp-server.c sftp-common.c sftp-server-main.c
SRCS+=	fatal.c
SRCS+=	${SRCS_BASE} ${SRCS_UTL}

PROG=	sftp-server
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec
MAN=	sftp-server.8

LDADD+=	-lutil
DPADD+= ${LIBUTIL}

.include <bsd.prog.mk>
