#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.1.1.1 1995/12/14 06:52:51 tholo Exp $

PROG=	ksrvtgt
CFLAGS+=-DKERBEROS
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-lkrb -ldes
BINDIR=	/usr/bin

.include <bsd.prog.mk>
