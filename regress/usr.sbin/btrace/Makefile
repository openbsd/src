# $OpenBSD: Makefile,v 1.5 2020/09/14 18:45:19 jasper Exp $

BTRACE?=		 /usr/sbin/btrace

# scripts that don't need /dev/dt
BT_LANG_SCRIPTS=	arithm beginend boolean comments delete exit map \
			map-unnamed maxoperand min+max+sum multismts nsecs+var

BT_KERN_SCRIPTS=

${BT_LANG_SCRIPTS}:
	${BTRACE} ${.CURDIR}/${.TARGET}.bt 2>/dev/null | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

REGRESS_TARGETS=${BT_LANG_SCRIPTS}
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>

