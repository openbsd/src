#	$OpenBSD: Makefile,v 1.12 2013/07/02 13:32:38 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	sftp
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	sftp.1

SRCS=	sftp.c sftp-client.c sftp-common.c sftp-glob.c

.include <bsd.prog.mk>

LDADD+=	-ledit -ltermcap -lutil
DPADD+= ${LIBEDIT} ${LIBTERMCAP} ${LIBUTIL}
