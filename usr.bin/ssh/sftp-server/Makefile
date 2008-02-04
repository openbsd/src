#	$OpenBSD: Makefile,v 1.7 2008/02/04 21:53:00 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	sftp-server
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec
MAN=	sftp-server.8

SRCS=	sftp-server.c sftp-common.c sftp-server-main.c

.include <bsd.prog.mk>
