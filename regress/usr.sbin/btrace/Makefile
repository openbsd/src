# $OpenBSD: Makefile,v 1.1 2020/01/21 16:27:17 mpi Exp $

BTRACE?=		 ${.CURDIR}/../../../usr.sbin/btrace/obj/btrace

# scripts that don't need /dev/dt
BT_LANG_SCRIPTS=	arithm beginend comments delete exit map maxoperand \
			multismts nsecs+var

BT_KERN_SCRIPTS=

${BT_LANG_SCRIPTS}:
	${BTRACE} ${.CURDIR}/${.TARGET}.bt 2>/dev/null | \
		diff -u ${.CURDIR}/${.TARGET}.ok /dev/stdin

REGRESS_TARGETS=${BT_LANG_SCRIPTS}
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>

