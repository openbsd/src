#	$OpenBSD: Makefile,v 1.23 2021/08/09 07:13:54 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	scp.c
SRCS+=	fatal.c atomicio.c progressmeter.c utf8.c
SRCS+=	sftp-common.c sftp-client.c sftp-glob.c
SRCS+=	${SRCS_BASE}

PROG=	scp

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lutil
DPADD+= ${LIBUTIL}
