#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:50:17 deraadt Exp $

PROG=	nfsstat
CFLAGS+=-DNFS
DPADD=  ${LIBKVM}
LDADD= -lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
