#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$OpenBSD: Makefile,v 1.3 1997/12/12 20:44:37 beck Exp $

PROG=	smtpd
CFLAGS+=-I${.CURDIR}/../src
SRCS= smtpd.c address_check.c 		
BINOWN=	root
BINGRP=	daemon
BINMODE=500
BINDIR=	/usr/libexec
MAN=    smtpd.8
.PATH:	${.CURDIR}/../src

.include <bsd.prog.mk>
