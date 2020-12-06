# $OpenBSD: Makefile,v 1.7 2020/12/06 13:32:18 bluhm Exp $

BTRACE?=		 /usr/sbin/btrace

# scripts that don't need /dev/dt
BT_LANG_SCRIPTS=	arithm beginend boolean comments delete exit map \
			map-unnamed maxoperand min+max+sum multismts nsecs+var

BT_KERN_SCRIPTS=

${BT_LANG_SCRIPTS}:
	${BTRACE} ${.CURDIR}/${.TARGET}.bt | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

REGRESS_TARGETS=${BT_LANG_SCRIPTS}
REGRESS_EXPECTED_FAILURES=maxoperand
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>

