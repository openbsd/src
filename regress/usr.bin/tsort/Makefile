# $OpenBSD: Makefile,v 1.3 2001/07/09 10:48:03 espie Exp $

NOMAN=
NOPROG=

regress: t1 t2 t3 t4 #t6

t1: t1.out
	perl ${.CURDIR}/tsort-check t1.out ${.CURDIR}/pairs1

t2: t2.out
	perl ${.CURDIR}/normalize-cycles <t2.out|head -1|diff - ${.CURDIR}/t2.canonical

t3: t3.out pairs3
	time perl ${.CURDIR}/tsort-check t3.out pairs3

t4: t4.out pairs4
	time perl ${.CURDIR}/tsort-check t4.out pairs4

t6:
	tsort <pairs6

t1.out:
	tsort <${.CURDIR}/pairs1 >$@

t2.out:
	cat ${.CURDIR}/pairs1 ${.CURDIR}/pairs2 |tsort -l >/dev/null 2>$@

t3.out: pairs3
	tsort <pairs3 >$@

t4.out: pairs4
	tsort <pairs4 >$@

pairs3:
	perl ${.CURDIR}/build-tree 4 8 >$@

pairs4:
	perl ${.CURDIR}/build-tree 5 10 >$@

.PHONY: t1 t2 regress
CLEANFILES+=t1.out t2.out t3.out t4.out pairs3 pairs4

.include <bsd.prog.mk>
