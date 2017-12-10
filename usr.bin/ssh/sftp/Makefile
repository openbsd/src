#	$OpenBSD: Makefile,v 1.14 2017/12/10 19:37:57 deraadt Exp $

.PATH:		${.CURDIR}/..

SRCS=	sftp.c sftp-client.c sftp-common.c sftp-glob.c
SRCS+=	atomicio.c cleanup.c fatal.c log.c misc.c progressmeter.c \
	sshbuf-getput-basic.c sshbuf.c ssherr.c uidswap.c utf8.c xmalloc.c

PROG=	sftp
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-ledit -ltermcap -lutil
DPADD+= ${LIBEDIT} ${LIBTERMCAP} ${LIBUTIL}
