#	$OpenBSD: Makefile,v 1.5 2002/06/17 18:01:06 deraadt Exp $

PROG=	ccdconfig
MAN=	ccdconfig.8

# Only permit root to use -g 
#BINGRP= kmem
#BINMODE=2555

DPADD=	${LIBKVM} ${LIBUTIL}
LDADD=	-lkvm -lutil

.include <bsd.prog.mk>
