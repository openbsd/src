#	$OpenBSD: Makefile.inc,v 1.1 2017/07/03 18:19:55 bluhm Exp $

TOPSRC=		${.CURDIR}/../../../../sbin/isakmpd
TOPOBJ!!=	${MAKE} -C ${TOPSRC} -V .OBJDIR
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/openbsd -I${TOPOBJ} -Wall
DEBUG=		-g

.PHONY: generated

${PROG}: generated

generated:
	${MAKE} -C ${TOPSRC} generated

.PATH:		${.CURDIR}/../common
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/openbsd ${TOPOBJ}
