#	$OpenBSD: Makefile,v 1.1 2004/01/29 20:11:09 millert Exp $

PROG=fifotest
FIFO=fi.fo.fum
CLEANFILES+=${FIFO}
CFLAGS+= -Wall

REGRESS_TARGETS=runtest

runtest: ${PROG}
	./${PROG} ${FIFO} | diff - ${.CURDIR}/${PROG}.out

.include <bsd.regress.mk>
