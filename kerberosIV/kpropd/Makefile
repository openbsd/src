#	$Id: Makefile,v 1.2 1997/06/29 07:21:19 deraadt Exp $

PROG=	kpropd
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
MAN=	kpropd.8
BINDIR=/usr/libexec

.include <bsd.prog.mk>
