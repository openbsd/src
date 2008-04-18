#	$OpenBSD: Makefile,v 1.11 2008/04/18 12:32:11 djm Exp $

.PATH:		${.CURDIR}/..

PROG=	sftp
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	sftp.1

SRCS=	sftp.c sftp-client.c sftp-common.c sftp-glob.c

.include <bsd.prog.mk>

LDADD+=	-ledit -ltermcap -lutil
