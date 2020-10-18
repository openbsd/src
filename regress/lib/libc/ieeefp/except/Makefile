#	$OpenBSD: Makefile,v 1.6 2020/10/18 17:40:06 kettenis Exp $

PROG=except

REGRESS_TARGETS+= fltdiv fltinv fltovf fltund

.if ${MACHINE} == arm64 || ${MACHINE} == armv7
# Floating-point exceptions are optional and absent on most hardware
REGRESS_EXPECTED_FAILURES+= fltdiv fltinv fltovf fltund
.endif

fltdiv: ${PROG}
	./${PROG} fltdiv

fltinv: ${PROG}
	./${PROG} fltinv

fltovf: ${PROG}
	./${PROG} fltovf

fltund: ${PROG}
	./${PROG} fltund

.include <bsd.regress.mk>
