#	$OpenBSD: Makefile,v 1.8 2010/01/13 01:40:16 djm Exp $

.PATH:		${.CURDIR}/..

PROG=	sftp-server
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec
MAN=	sftp-server.8

SRCS=	sftp-server.c sftp-common.c sftp-server-main.c

LDADD+=	-lutil

.include <bsd.prog.mk>
