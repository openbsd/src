#	$OpenBSD: Makefile.inc,v 1.3 2017/07/05 15:31:45 bluhm Exp $

TOPSRC=		${.CURDIR}/../../../../lib/libedit
TOPOBJ!!=	${MAKE} -C ${TOPSRC} -V .OBJDIR
CFLAGS+=	-I${TOPSRC} -I${TOPOBJ}
CLEANFILES+=	stamp-generated

${PROG}: stamp-generated

stamp-generated:
	${MAKE} -C ${TOPSRC} generated
	date >$@
