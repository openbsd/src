# $OpenBSD: Makefile,v 1.5 2018/11/12 09:48:48 martijn Exp $

REGRESS_TARGETS=t1 t2 t3 t4 t5 t6 t7 t8 t9-utf8 t9-c

t1:
	join -t "	" -a 1 -a 2 -e '(unknown)' -o 0,1.2,2.2 ${.CURDIR}/phone ${.CURDIR}/fax > ${*}.log
	@cmp -s ${.CURDIR}/${*}.out ${*}.log || \
		(echo "XXX ${*} failed" && false)

t2:
	join ${.CURDIR}/fa1 ${.CURDIR}/fb1 > ${*}.log
	@cmp -s ${.CURDIR}/${*}.out ${*}.log || \
		(echo "XXX ${*} failed" && false)

t3:
	join ${.CURDIR}/fa2 ${.CURDIR}/fb2 > ${*}.log
	@cmp -s ${.CURDIR}/${*}.out ${*}.log || \
		(echo "XXX ${*} failed" && false)

t4:
	join -t, -o0,1.2,1.3,2.2,2.3 -a1 ${.CURDIR}/t4a.in ${.CURDIR}/t4b.in > ${*}.log
	@cmp -s ${.CURDIR}/${*}.out ${*}.log || \
		(echo "XXX ${*} failed" && false)

t5:
	join -t, -o0,1.2,1.3,2.2,2.3 -a2  ${.CURDIR}/t4a.in ${.CURDIR}/t4b.in > ${*}.log
	@cmp -s ${.CURDIR}/${*}.out ${*}.log || \
		(echo "XXX ${*} failed" && false)

t6:
	join -t, -o0,1.2,1.3,2.2,2.3 -a1 -a2 ${.CURDIR}/t4a.in ${.CURDIR}/t4b.in > ${*}.log
	@cmp -s ${.CURDIR}/${*}.out ${*}.log || \
		(echo "XXX ${*} failed" && false)

t7:
	join -j 2 ${.CURDIR}/t7a.in ${.CURDIR}/t7b.in > ${*}.log
	@cmp -s ${.CURDIR}/${*}.out ${*}.log || \
		(echo "XXX ${*} failed" && false)

t8:
	join -j 2 -e empty ${.CURDIR}/t8a.in ${.CURDIR}/t8b.in > ${*}.log
	@cmp -s ${.CURDIR}/${*}.out ${*}.log || \
		(echo "XXX ${*} failed" && false)


# LC_ALL takes precedence over LC_CTYPE.
t9-utf8:
	LC_ALL=en_US.UTF-8 join -t‐ ${.CURDIR}/t9a.in ${.CURDIR}/t9b.in > ${*}.log
	@cmp -s ${.CURDIR}/${*}.out ${*}.log || \
		(echo "XXX ${*} failed" && false)

t9-c:
	! LC_ALL=C join -t‐ ${.CURDIR}/t9a.in ${.CURDIR}/t9b.in > ${*}.log 2>&1
	@cmp -s ${.CURDIR}/${*}.out ${*}.log || \
		(echo "XXX ${*} failed" && false)

clean:
	rm -f *.log

.include <bsd.regress.mk>
