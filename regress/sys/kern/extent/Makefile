# $OpenBSD: Makefile,v 1.3 2015/06/29 19:56:58 miod Exp $
# $NetBSD: Makefile,v 1.5 2002/09/18 04:16:02 lukem Exp $

PROG=		extest
SRCS=		extest.c subr_extent.c
CPPFLAGS+=	-D_EXTENT_TESTING -DDIAGNOSTIC
CLEANFILES+=	extest.c extest.out extest.exp

.PATH:		${.CURDIR}/../../../../sys/kern

REGRESS_TARGETS=	do-extent

MINUSONE!=	awk 'BEGIN{printf("%lx\n",-1)}' < /dev/null

do-extent: ${PROG}
	sed 's/ffffffffffffffff/${MINUSONE}/' < ${.CURDIR}/extest.exp > extest.exp
	./${PROG} >extest.out
	tail +5 extest.exp | diff - extest.out

extest.c: extest.awk tests
	awk -f ${.CURDIR}/extest.awk <${.CURDIR}/tests >extest.c

.include <bsd.regress.mk>
