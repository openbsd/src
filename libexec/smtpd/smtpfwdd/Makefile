#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$OpenBSD: Makefile,v 1.1 1997/12/12 05:55:22 beck Exp $

PROG=	smtpfwdd
CFLAGS+=-I${.CURDIR}/../src
SRCS= smtpfwdd.c 		
BINOWN=	root
BINGRP=	daemon
BINMODE=500
BINDIR=	/usr/libexec
MAN=    smtpfwdd.8
.PATH:	${.CURDIR}/../src

.include <bsd.prog.mk>
