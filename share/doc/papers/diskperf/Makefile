#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:14:56 niklas Exp $


DIR=	papers/diskperf
SRCS=	abs.ms motivation.ms equip.ms methodology.ms tests.ms results.ms \
	conclusions.ms appendix.ms
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
