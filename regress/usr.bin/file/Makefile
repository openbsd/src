# $OpenBSD: Makefile,v 1.8 2017/06/28 17:28:39 nicm Exp $

FILE=file

REGRESS_TARGETS=t0  t1  t2  t3  t4  t5  t6  t7  \
		t8  t9  t10 t11 t15 t17 t18 t19 \
		t20 t21 t22 t30 t31 t32 t33 stdin

# .in:  input file
# .out: desired result

all: ${REGRESS_TARGET}

.SUFFIXES: .in

.in:
	@echo ${*}
	@${FILE} ${.CURDIR}/${*}.in | \
		sed -e "s@${.CURDIR}/@@" | \
		diff - ${.CURDIR}/${*}.out || \
		(echo "XXX ${*} failed" && false)

stdin:
	@echo ${*}
	@${FILE} - <t0.in | \
		diff - ${.CURDIR}/${*}.out || \
		(echo "XXX ${*} failed" && false)

clean:

.include <bsd.regress.mk>
