#	$NetBSD: Makefile,v 1.2 1996/05/16 07:11:17 thorpej Exp $

PROG=	ccdconfig
MAN=	ccdconfig.8

BINGRP=	kmem
BINMODE=2555

DPADD=	${LIBKVM} ${LIBUTIL}
LDADD=	-lkvm -lutil

.include <bsd.prog.mk>
