# $OpenBSD: Makefile,v 1.1 2003/07/02 07:07:49 pvalchev Exp $

REGRESS_TARGETS=t1 t2 t3 t4 t5 t6 t7 t8 t9 t10

t1:
	grep t.s ${.CURDIR}/in | diff - ${.CURDIR}/t1.out

t2:
	grep -w separated ${.CURDIR}/in | diff - ${.CURDIR}/t1.out

t3:
	grep ^Beginning ${.CURDIR}/in | diff - ${.CURDIR}/t3.out

t4:
	grep -i end$$ ${.CURDIR}/in | diff - ${.CURDIR}/t4.out

t5:
	egrep '\?|\*$$' ${.CURDIR}/in | diff - ${.CURDIR}/t5.out

t6:
	grep -v [l] ${.CURDIR}/in | diff - ${.CURDIR}/t6.out

t7:
	grep -x line ${.CURDIR}/in | diff - ${.CURDIR}/t7.out

t8:
	fgrep line. ${.CURDIR}/in | diff - ${.CURDIR}/empty

t9:
	grep non.existent ${.CURDIR}/in | diff - ${.CURDIR}/empty

t10:
	grep -s foo nonexistentfile | diff - ${.CURDIR}/empty

.PHONY: t1 t2 t3 t4 t5 t6 t7 t8 t9 t10

.include <bsd.regress.mk>
