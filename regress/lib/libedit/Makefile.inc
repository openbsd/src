#	$OpenBSD: Makefile.inc,v 1.2 2017/07/04 21:56:05 bluhm Exp $

TOPSRC=		${.CURDIR}/../../../../lib/libedit
TOPOBJ!!=	${MAKE} -C ${TOPSRC} -V .OBJDIR
CFLAGS+=	-I${TOPSRC} -I${TOPOBJ}

.PHONY: generated

${PROG}: generated

generated:
	${MAKE} -C ${TOPSRC} generated
