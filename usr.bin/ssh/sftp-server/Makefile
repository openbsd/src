#	$OpenBSD: Makefile,v 1.10 2017/12/10 19:37:57 deraadt Exp $

.PATH:		${.CURDIR}/..

SRCS=	sftp-server.c sftp-common.c sftp-server-main.c
SRCS+=	addrmatch.c fatal.c log.c match.c misc.c sshbuf-getput-basic.c \
	sshbuf.c ssherr.c uidswap.c xmalloc.c

PROG=	sftp-server
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec
MAN=	sftp-server.8

LDADD+=	-lutil
DPADD+= ${LIBUTIL}

.include <bsd.prog.mk>
