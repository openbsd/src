#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.2 1997/12/12 05:30:16 art Exp $

PROG=	kinit
DPADD=	${LIBKRB} ${LIBDES}
LDADD=	-g -lkrb -ldes
BINDIR=	/usr/bin
CFLAGS += -g

.include <bsd.prog.mk>
