# $OpenBSD: Makefile,v 1.12 2021/09/01 13:22:36 mpi Exp $

BTRACE?=		 /usr/sbin/btrace

# scripts that don't need /dev/dt
BT_LANG_SCRIPTS=	arithm beginend boolean comments delete exit if map \
			map-unnamed maxoperand min+max+sum multismts nsecs+var \
			precedence print read-map-after-clear

BT_KERN_SCRIPTS=

REGRESS_EXPECTED_FAILURES=	run-maxoperand

.for b in ${BT_LANG_SCRIPTS}
REGRESS_TARGETS+=		run-$b
run-$b:
	${BTRACE} ${.CURDIR}/$b.bt | \
		diff -u ${.CURDIR}/$b.ok /dev/stdin
.endfor

.include <bsd.regress.mk>
