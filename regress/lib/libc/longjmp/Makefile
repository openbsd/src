#	$OpenBSD: Makefile,v 1.3 2002/02/18 11:27:45 art Exp $
PROG=	longjmp

do-longjmp: ${PROG}
	./longjmp

do-_longjmp: ${PROG}
	./longjmp -_

REGRESSTARGETS=do-longjmp do-_longjmp
.PHONY: ${REGRESSTARGETS}

.include <bsd.regress.mk>
