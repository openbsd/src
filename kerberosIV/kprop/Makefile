#	$Id: Makefile,v 1.3 1998/02/18 12:55:16 art Exp $

PROG=	kprop
LDADD+=	-lkrb -ldes
DPADD+=	${LIBKRB} ${LIBDES}
MAN=	kprop.8
CFLAGS+=-Wall

.include <bsd.prog.mk>
