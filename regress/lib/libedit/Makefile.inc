#	$OpenBSD: Makefile.inc,v 1.1 2017/07/03 18:19:55 bluhm Exp $

TOPSRC=		${.CURDIR}/../../../../lib/libedit
TOPOBJ!!=	${MAKE} -C ${TOPSRC} -V .OBJDIR
CLAGS+=		-I${TOPSRC} -I${TOPOBJ}

.PHONY: generated

${PROG}: generated

generated:
	${MAKE} -C ${TOPSRC} generated
