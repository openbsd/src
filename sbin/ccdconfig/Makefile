#	$OpenBSD: Makefile,v 1.6 2003/06/10 16:41:28 deraadt Exp $

PROG=	ccdconfig
MAN=	ccdconfig.8

# Only permit root to use -g
#BINGRP= kmem
#BINMODE=2555

DPADD=	${LIBKVM} ${LIBUTIL}
LDADD=	-lkvm -lutil

.include <bsd.prog.mk>
