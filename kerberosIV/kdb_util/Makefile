#	from @(#)Makefile	8.1 (Berkeley) 6/1/93
#	$Id: Makefile,v 1.2 1997/12/18 08:50:14 art Exp $

PROG=	kdb_util
SRCS=	kdb_util.c
.PATH:	${.CURDIR}/../kdb_edit
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
MAN=	kdb_util.8

.include <bsd.prog.mk>
