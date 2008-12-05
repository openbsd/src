#	$OpenBSD: Makefile,v 1.1 2008/12/05 03:28:37 gilles Exp $

.PATH:		${.CURDIR}/..

PROG=	smtpctl
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	smtpctl.8

SRCS=	smtpctl.c parser.c buffer.c imsg.c log.c
LDFLAGS=	-lutil
.include <bsd.prog.mk>
