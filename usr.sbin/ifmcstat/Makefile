PROG=	ifmcstat
MAN=	ifmcstat.8

BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm
DPADD=	${LIBKVM}

CPPFLAGS+=	-DINET6

.include <bsd.prog.mk>
