#	$OpenBSD: Makefile,v 1.19 2026/02/06 23:31:29 dtucker Exp $

.PATH:		${.CURDIR}/..

SRCS=	sftp.c sftp-client.c sftp-common.c sftp-glob.c sftp-usergroup.c
SRCS+=	atomicio.c cleanup.c fatal.c progressmeter.c utf8.c
SRCS+=	ssherr-nolibcrypto.c
SRCS+=	${SRCS_BASE}

PROG=	sftp

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-ledit -ltermcap -lutil
DPADD+= ${LIBEDIT} ${LIBTERMCAP} ${LIBUTIL}
