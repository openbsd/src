#	$OpenBSD: Makefile,v 1.1 2008/10/10 14:33:34 millert Exp $
#	$NetBSD: Makefile,v 1.1 2005/04/04 16:48:45 peter Exp $

SED=	/usr/bin/sed

regress:
	sh ${.CURDIR}/sedtest.sh ${SED} sedtest.out
	diff ${.CURDIR}/sedtest.expected sedtest.out
	printf ":abcd: : :\n\n" | ${SED} -f ${.CURDIR}/hanoi.sed > hanoi.out
	diff ${.CURDIR}/hanoi.expected hanoi.out
	${SED} -f ${.CURDIR}/math.sed ${.CURDIR}/math.in > math.out
	diff ${.CURDIR}/math.expected math.out
	echo ________________________X___ | ${SED} -nf ${.CURDIR}/sierpinski.sed > sierpinski.out
	diff ${.CURDIR}/sierpinski.expected sierpinski.out

CLEANFILES+=*.out lines* script*

.include <bsd.regress.mk>
