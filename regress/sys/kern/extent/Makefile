# $OpenBSD: Makefile,v 1.4 2017/07/25 12:49:29 bluhm Exp $
# $NetBSD: Makefile,v 1.5 2002/09/18 04:16:02 lukem Exp $

PROG=		extest
SRCS=		extest.c subr_extent.c
CPPFLAGS+=	-D_EXTENT_TESTING -DDIAGNOSTIC
CLEANFILES+=	extest.c extest.out

.PATH:		${.CURDIR}/../../../../sys/kern

REGRESS_TARGETS=	do-extent

MINUSONE!=	awk 'BEGIN{printf("%lx\n",-1)}' < /dev/null

do-extent: ${PROG}
	./${PROG} >extest.out
	sed -n 's/ffffffffffffffff/${MINUSONE}/;5,$$p' ${.CURDIR}/extest.exp |\
	    diff - extest.out

extest.c: extest.awk tests
	awk -f ${.CURDIR}/extest.awk <${.CURDIR}/tests >extest.c

.include <bsd.regress.mk>
