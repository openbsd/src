#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.2 1997/12/17 10:21:26 art Exp $

PROG=	kdb_edit
SRCS=	kdb_edit.c
CFLAGS+=-I${.CURDIR}
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
MAN=	kdb_edit.8

.include <bsd.prog.mk>
