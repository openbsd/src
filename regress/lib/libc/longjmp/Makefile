PROG=	longjmp

do-longjmp: ${PROG}
	./longjmp
do-_longjmp: ${PROG}
	./longjmp -_

REGRESSTARGETS=do-longjmp do-_longjmp

.include <bsd.regress.mk>
