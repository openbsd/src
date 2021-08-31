# $OpenBSD: Makefile,v 1.11 2021/08/31 12:51:56 mpi Exp $

BTRACE?=		 /usr/sbin/btrace

# scripts that don't need /dev/dt
BT_LANG_SCRIPTS=	arithm beginend boolean comments delete exit if map \
			map-unnamed maxoperand min+max+sum multismts nsecs+var \
			precedence print

BT_KERN_SCRIPTS=

REGRESS_EXPECTED_FAILURES=	run-maxoperand

.for b in ${BT_LANG_SCRIPTS}
REGRESS_TARGETS+=		run-$b
run-$b:
	${BTRACE} ${.CURDIR}/$b.bt | \
		diff -u ${.CURDIR}/$b.ok /dev/stdin
.endfor

.include <bsd.regress.mk>
