#	$OpenBSD: Makefile,v 1.2 2008/10/13 13:27:33 millert Exp $
#	$NetBSD: Makefile,v 1.1 2005/04/04 16:48:45 peter Exp $

SED=	/usr/bin/sed

REGRESS_TARGETS= sedtest hanoi math sierpinski

sedtest:
	sh ${.CURDIR}/$@.sh ${SED} $@.out
	diff ${.CURDIR}/$@.expected $@.out

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
