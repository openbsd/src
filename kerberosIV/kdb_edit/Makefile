#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.1.1.1 1995/12/14 06:52:42 tholo Exp $

PROG=	kdb_edit
SRCS=	kdb_edit.c maketime.c
CFLAGS+=-I${.CURDIR}
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
MAN=	kdb_edit.8

.include <bsd.prog.mk>
