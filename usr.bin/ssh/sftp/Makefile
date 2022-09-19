#	$OpenBSD: Makefile,v 1.18 2022/09/19 10:46:00 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	sftp.c sftp-client.c sftp-common.c sftp-glob.c sftp-usergroup.c
SRCS+=	atomicio.c cleanup.c fatal.c progressmeter.c utf8.c
SRCS+=	${SRCS_BASE}

PROG=	sftp

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-ledit -ltermcap -lutil
DPADD+= ${LIBEDIT} ${LIBTERMCAP} ${LIBUTIL}
