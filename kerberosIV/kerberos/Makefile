#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.1.1.1 1995/12/14 06:52:52 tholo Exp $

PROG=	kerberos
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
MAN=	kerberos.8

.include <bsd.prog.mk>
