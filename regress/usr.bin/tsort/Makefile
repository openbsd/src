# $OpenBSD: Makefile,v 1.7 2002/09/02 20:01:44 avsm Exp $

REGRESS_TARGETS=t1 t2 t3 t4 t5 t6
REGRESS_SLOW_TARGETS=t4

t1: t1.out
	perl ${.CURDIR}/tsort-check t1.out ${.CURDIR}/pairs1

t2: t2.out
	perl ${.CURDIR}/normalize-cycles <t2.out|head -1|diff - ${.CURDIR}/t2.canonical

t3: t3.out pairs3
	perl ${.CURDIR}/tsort-check t3.out pairs3

t4: t4.out pairs4
	perl ${.CURDIR}/tsort-check t4.out pairs4

t5: t5.out
	perl ${.CURDIR}/tsort-check t5.out ${.CURDIR}/pairs5

t6: t6.out
	perl ${.CURDIR}/tsort-check t6.out ${.CURDIR}/pairs6

t1.out:
	tsort <${.CURDIR}/pairs1 >$@ 2>&1

t2.out:
	cat ${.CURDIR}/pairs1 ${.CURDIR}/pairs2 |tsort -l >/dev/null 2>$@

t3.out: pairs3
	tsort <pairs3 >$@ 2>&1

t4.out: pairs4
	tsort <pairs4 >$@ 2>&1

t5.out: pairs5
	tsort <${.CURDIR}/pairs5 >$@ 2>&1

t6.out: pairs6
	tsort <${.CURDIR}/pairs6 >$@ 2>&1

pairs3:
	perl ${.CURDIR}/build-tree 4 8 >$@

pairs4:
	perl ${.CURDIR}/build-tree 5 10 >$@

.PHONY: t1 t2
CLEANFILES+=t1.out t2.out t3.out t4.out t5.out t6.out pairs3 pairs4

.include <bsd.regress.mk>

