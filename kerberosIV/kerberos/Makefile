#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.2 1997/11/28 12:48:46 art Exp $

PROG=	kerberos
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
MAN=	kerberos.8

CFLAGS+=-I${.CURDIR}

.include <bsd.prog.mk>
