#	$OpenBSD: Makefile,v 1.2 1996/03/27 19:32:47 niklas Exp $
#	$NetBSD: Makefile,v 1.5 1996/03/03 17:21:25 thorpej Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	nfsstat
CFLAGS+=-DNFS
DPADD=  ${LIBKVM}
LDADD= -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
