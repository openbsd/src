#	$OpenBSD: Makefile,v 1.22 2021/08/02 23:38:27 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	scp.c
SRCS+=	atomicio.c cleanup.c fatal.c progressmeter.c utf8.c
SRCS+=	sftp-common.c sftp-client.c sftp-glob.c
SRCS+=	${SRCS_BASE}

PROG=	scp

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lutil
DPADD+= ${LIBUTIL}
