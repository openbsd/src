#	$OpenBSD: Makefile,v 1.3 2011/06/23 11:53:57 schwarze Exp $
#	$NetBSD: Makefile,v 1.1 2005/04/04 16:48:45 peter Exp $

SED=	/usr/bin/sed

REGRESS_TARGETS= sedtest substitute hanoi math sierpinski

sedtest:
	sh ${.CURDIR}/$@.sh ${SED} $@.out
	diff ${.CURDIR}/$@.expected $@.out

substitute:
	sh ${.CURDIR}/$@.sh

hanoi:
	${SED} -f ${.CURDIR}/$@.sed ${.CURDIR}/$@.in > $@.out
	diff ${.CURDIR}/$@.expected $@.out

math:
	${SED} -f ${.CURDIR}/$@.sed ${.CURDIR}/$@.in > $@.out
	diff ${.CURDIR}/$@.expected $@.out

sierpinski:
	${SED} -nf ${.CURDIR}/$@.sed ${.CURDIR}/$@.in > $@.out
	diff ${.CURDIR}/$@.expected $@.out

CLEANFILES+=*.out lines* script*

.include <bsd.regress.mk>
