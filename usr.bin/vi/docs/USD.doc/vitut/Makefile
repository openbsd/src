#	$OpenBSD: Makefile,v 1.5 2004/01/30 23:39:22 jmc Exp $


DIR=	usd/12.vi
SRCS=	vi.in vi.chars
EXTRA=	vi.apwh.ms vi.summary
MACROS=	-ms
CLEANFILES+=summary.* viapwh.*

paper.ps: ${SRCS} summary.ps viapwh.ps
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}
paper.txt: ${SRCS} summary.txt viapwh.txt
	${TBL} ${SRCS} | ${ROFF} -Tascii > ${.TARGET}

summary.ps: vi.summary
	${TBL} vi.summary | ${ROFF} > ${.TARGET}
summary.txt: vi.summary
	${TBL} vi.summary | ${ROFF} -Tascii > ${.TARGET}

viapwh.ps: vi.apwh.ms
	${ROFF} vi.apwh.ms > ${.TARGET}
viapwh.txt: vi.apwh.ms
	${ROFF} -Tascii vi.apwh.ms > ${.TARGET}

.include <bsd.doc.mk>
