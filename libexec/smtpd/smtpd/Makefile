#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$OpenBSD: Makefile,v 1.4 1998/06/03 08:56:56 beck Exp $

PROG=	smtpd
CFLAGS+=-I${.CURDIR}/../src
SRCS= smtpd.c address_check.c accumlog.c 		
BINOWN=	root
BINGRP=	daemon
BINMODE=500
BINDIR=	/usr/libexec
MAN=    smtpd.8
.PATH:	${.CURDIR}/../src

.include <bsd.prog.mk>
