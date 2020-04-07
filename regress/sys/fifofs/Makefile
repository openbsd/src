#	$OpenBSD: Makefile,v 1.2 2020/04/07 09:29:45 mpi Exp $

PROG=fifotest
FIFO=fi.fo.fum
CLEANFILES+=${FIFO}
CFLAGS+= -Wall

REGRESS_TARGETS=runtest

runtest: ${PROG}
	./${PROG} ${FIFO} | diff -u - ${.CURDIR}/${PROG}.out

.include <bsd.regress.mk>
