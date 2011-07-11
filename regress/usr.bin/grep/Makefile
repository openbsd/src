# $OpenBSD: Makefile,v 1.14 2011/07/11 20:35:41 tedu Exp $

REGRESS_TARGETS=t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12 t13 t14 t15 t16 t17 \
	t18 t19 t20 t21 t22 t23 t24

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

t19:
	grep  -w 'a.' ${.CURDIR}/t19.in | diff - ${.CURDIR}/t19.out
	fgrep -w -e 'aa' -e 'a_' ${.CURDIR}/t19.in | diff - ${.CURDIR}/t19.out

t20:
	echo '='   | grep  -w  '='  | diff - /dev/null
	echo '='   | fgrep -w  '='  | diff - /dev/null
	echo 'a='  | grep  -w  'a=' | diff - /dev/null
	echo 'a='  | fgrep -w  'a=' | diff - /dev/null
	echo '=b'  | grep  -w  '=b' | diff - /dev/null
	echo '=b'  | fgrep -w  '=b' | diff - /dev/null
	echo 'a=b' | grep  -qw 'a=b'
	echo 'a=b' | fgrep -qw 'a=b'
	echo '+'   | grep  -w  '+'  | diff - /dev/null
	echo '+'   | fgrep -w  '+'  | diff - /dev/null
	echo 'a+'  | grep  -w  'a+' | diff - /dev/null
	echo 'a+'  | fgrep -w  'a+' | diff - /dev/null
	echo '+b'  | grep  -w  '+b' | diff - /dev/null
	echo '+b'  | fgrep -w  '+b' | diff - /dev/null
	echo 'a+b' | grep  -qw 'a+b'
	echo 'a+b' | fgrep -qw 'a+b'

t21:
	grep -l D.e ${.CURDIR}/in | diff - ${.CURDIR}/t21.out
	egrep -l D.e ${.CURDIR}/in | diff - ${.CURDIR}/t21.out
	grep -l '.*D.e' ${.CURDIR}/in | diff - ${.CURDIR}/t21.out
	egrep -l '.*D.e' ${.CURDIR}/in | diff - ${.CURDIR}/t21.out

t22:
	grep -l 'a.*b' ${.CURDIR}/t22.in

t23:
	egrep -w 'word1|word2|word3' ${.CURDIR}/t23.in

t24:
	grep -e '' < in | diff - in
	grep -x -e '' < in | diff - /dev/null
	grep -f /dev/null < in | diff - /dev/null


.PHONY: t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12 t13 t14 t15 t16 t17 t18 t19 t20
.PHONY: t21 t22 t23 t24

.include <bsd.regress.mk>
