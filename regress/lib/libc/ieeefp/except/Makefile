#	$OpenBSD: Makefile,v 1.5 2004/07/22 19:29:42 kettenis Exp $

PROG=except

REGRESS_TARGETS+= fltdiv fltinv fltovf fltund

fltdiv: ${PROG}
	./${PROG} fltdiv

fltinv: ${PROG}
	./${PROG} fltinv

fltovf: ${PROG}
	./${PROG} fltovf

fltund: ${PROG}
	./${PROG} fltund

.include <bsd.regress.mk>
