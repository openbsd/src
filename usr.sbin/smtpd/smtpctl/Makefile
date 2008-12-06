#	$OpenBSD: Makefile,v 1.2 2008/12/06 02:44:08 gilles Exp $

.PATH:		${.CURDIR}/..

PROG=	smtpctl
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	smtpctl.8

SRCS=	smtpctl.c parser.c buffer.c imsg.c log.c showqueue.c
LDFLAGS=	-lutil
.include <bsd.prog.mk>
