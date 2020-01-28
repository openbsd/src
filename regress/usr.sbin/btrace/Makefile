# $OpenBSD: Makefile,v 1.3 2020/01/28 16:39:51 mpi Exp $

BTRACE?=		 /usr/sbin/btrace

# scripts that don't need /dev/dt
BT_LANG_SCRIPTS=	arithm beginend comments delete exit map maxoperand \
			min+max+sum multismts nsecs+var

BT_KERN_SCRIPTS=

${BT_LANG_SCRIPTS}:
	${BTRACE} ${.CURDIR}/${.TARGET}.bt 2>/dev/null | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

REGRESS_TARGETS=${BT_LANG_SCRIPTS}
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>

