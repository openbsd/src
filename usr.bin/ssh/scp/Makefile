#	$OpenBSD: Makefile,v 1.24 2026/02/06 23:31:29 dtucker Exp $

.PATH:		${.CURDIR}/..

SRCS=	scp.c
SRCS+=	fatal.c atomicio.c progressmeter.c utf8.c
SRCS+=	sftp-common.c sftp-client.c sftp-glob.c
SRCS+=	ssherr-nolibcrypto.c
SRCS+=	${SRCS_BASE}

PROG=	scp

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lutil
DPADD+= ${LIBUTIL}
