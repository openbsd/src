#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.3 1998/01/23 08:32:07 art Exp $

PROG=	kerberos
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
MAN=	kerberos.8

BINDIR= /usr/libexec

CFLAGS+=-I${.CURDIR}

.include <bsd.prog.mk>
