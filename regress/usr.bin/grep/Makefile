# $OpenBSD: Makefile,v 1.9 2004/02/04 05:53:49 millert Exp $

REGRESS_TARGETS=t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12 t13 t14 t15 t16

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
	fgrep line. ${.CURDIR}/in | diff - /dev/null

t9:
	grep non.existent ${.CURDIR}/in | diff - /dev/null

t10:
	grep -s foo nonexistentfile | diff - /dev/null

t11:
	grep -C1 examples ${.CURDIR}/in | diff - ${.CURDIR}/t11.out

t12:
	egrep -f ${.CURDIR}/t1.out ${.CURDIR}/in | diff - ${.CURDIR}/t1.out

t13:
	grep 'abcd. abcd.' ${.CURDIR}/t13.in | diff - ${.CURDIR}/t13.out

t14:
	test X`grep -w -l u_int ${.CURDIR}/t14.in` = X${.CURDIR}/t14.in

t15:
	grep -w tu ${.CURDIR}/t15.in | diff - ${.CURDIR}/t15.out

t16:
	grep -w to ${.CURDIR}/t15.in | diff - ${.CURDIR}/t15.out

t17:
	grep -w '^foo' ${.CURDIR}/t17.in | diff - ${.CURDIR}/t17.out

t18:
	grep -w '^foo$$' ${.CURDIR}/t17.in | diff - ${.CURDIR}/t17.out

.PHONY: t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12 t13 t14 t15 t16 t17 t18

.include <bsd.regress.mk>
