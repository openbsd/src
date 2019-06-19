#	$OpenBSD: Makefile,v 1.17 2018/07/25 17:12:35 deraadt Exp $

.PATH:		${.CURDIR}/..

SRCS=	sftp.c sftp-client.c sftp-common.c sftp-glob.c
SRCS+=	atomicio.c cleanup.c fatal.c progressmeter.c utf8.c
SRCS+=	${SRCS_BASE}

PROG=	sftp

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-ledit -ltermcap -lutil
DPADD+= ${LIBEDIT} ${LIBTERMCAP} ${LIBUTIL}
