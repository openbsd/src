#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$OpenBSD: Makefile,v 1.2 1997/12/12 06:40:19 deraadt Exp $

PROG=	smtpd
CFLAGS+=-I${.CURDIR}/../src
SRCS= smtpd.c address_check.c 		
BINOWN=	root
BINGRP=	daemon
BINMODE=500
BINDIR=	/usr/libexec
NOMAN=
#MAN=    smtpd.8
#MLINKS= smtpd.8 smtpd.5
.PATH:	${.CURDIR}/../src

.include <bsd.prog.mk>
