# $OpenBSD: Makefile,v 1.14 2021/09/07 19:30:44 mpi Exp $

BTRACE?=		 /usr/sbin/btrace

# scripts that don't need /dev/dt
BT_LANG_SCRIPTS=	arithm beginend boolean comments delete exit if \
			map mapclear mapsyntax mmapzero ap-unnamed \
			maxoperand min+max+sum multismts nsecs+var \
			precedence print read-map-after-clear syntaxerror


BT_KERN_SCRIPTS=

REGRESS_EXPECTED_FAILURES=	run-maxoperand

.for b in ${BT_LANG_SCRIPTS}
REGRESS_TARGETS+=		run-$b
run-$b:
	${BTRACE} ${.CURDIR}/$b.bt 2>&1 | \
		diff -u ${.CURDIR}/$b.ok /dev/stdin
.endfor

.include <bsd.regress.mk>
