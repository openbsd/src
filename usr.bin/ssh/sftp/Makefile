#	$OpenBSD: Makefile,v 1.16 2018/01/08 15:37:24 markus Exp $

.PATH:		${.CURDIR}/..

SRCS=	sftp.c sftp-client.c sftp-common.c sftp-glob.c
SRCS+=	atomicio.c cleanup.c fatal.c progressmeter.c utf8.c
SRCS+=	${SRCS_BASE}

PROG=	sftp
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-ledit -ltermcap -lutil
DPADD+= ${LIBEDIT} ${LIBTERMCAP} ${LIBUTIL}
