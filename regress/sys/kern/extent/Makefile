# $OpenBSD: Makefile,v 1.2 2005/04/22 00:37:58 miod Exp $
# $NetBSD: Makefile,v 1.5 2002/09/18 04:16:02 lukem Exp $

PROG=		extest
SRCS=		extest.c subr_extent.c
CPPFLAGS+=	-D_EXTENT_TESTING -DDIAGNOSTIC
CLEANFILES+=	extest.c extest.out

.PATH:		${.CURDIR}/../../../../sys/kern

REGRESS_TARGETS=	do-extent

do-extent: ${PROG}
	./${PROG} >extest.out
	tail +5 ${.CURDIR}/extest.exp | diff - extest.out

extest.c: extest.awk tests
	awk -f ${.CURDIR}/extest.awk <${.CURDIR}/tests >extest.c

.include <bsd.regress.mk>
