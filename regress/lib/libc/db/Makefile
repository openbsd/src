#	$OpenBSD: Makefile,v 1.9 2002/08/21 22:49:41 wcobb Exp $
#	$NetBSD: Makefile,v 1.11 1995/12/12 01:54:15 cgd Exp $

PROG=	dbtest

# add -DSTATISTICS to CFLAGS to get usage statistics.  Note that
# for this to work, libc must be compiled with -DSTATISTICS as well
CFLAGS+= -g -D__DBINTERFACE_PRIVATE -DDEBUG
CLEANFILES+= t1 t2 t3 log

DBTARGETS=1 2 3 4 5 6 7 8 9 10 11 12 13 20

.for DT in ${DBTARGETS}
db-${DT}: ${PROG}
	sh ${.CURDIR}/run.test ${DT}
REGRESSTARGETS+=db-${DT}
.PHONY: db-${DT}
.endfor

REGRESSSLOWTARGETS+=db-3 db-12 db-20

.include <bsd.regress.mk>
