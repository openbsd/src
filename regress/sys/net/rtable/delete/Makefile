#	$OpenBSD: Makefile,v 1.1 2015/11/04 09:45:52 mpi Exp $

PROG= 	delete
SRCS= 	main.c util.c rtable.c

REGRESS_TARGETS+=	fullfeed

${REGRESS_TARGETS}: ${PROG}
	${PROG} ${TOPDIR}/inet6-${.TARGET}.txt

.include <bsd.regress.mk>
