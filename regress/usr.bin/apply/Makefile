# $OpenBSD: Makefile,v 1.2 2018/04/01 11:07:52 bluhm Exp $

APPLY?=		/usr/bin/apply
CLEANFILES=	*.res

REGRESS_TARGETS=t1 t2 t3 t4 t5 t6

# .in: input file
# .out: desired output

# t1: uses arguments multiple times (from FreeBSD bin/95079)
# t2: overflows ARG_MAX (from FreeBSD bin/95079)
# t3: debugs -0 call
# t4: debugs -2 call
# t5: uses magic character '&'
# t6: uses magic character ' ' with command starting with a number

t1:
	@echo ${*}
	${APPLY} "echo %1 %1 %1 %1" `cat ${.CURDIR}/${*}.in` > ${*}.res
	@cmp -s ${.CURDIR}/${*}.out ${*}.res || \
	    (echo "XXX ${*} failed" && false)

t2:
	@echo ${*}
	ARG_MAX=`getconf ARG_MAX`;\
	    ARG_MAX_HALF=$$((ARG_MAX / 2)); \
	    ! ${APPLY} "echo %1 %1 %1" \
	    `jot $$ARG_MAX_HALF 1 1 | tr -d '\n'` > ${*}.res 2>&1

t3:
	@echo ${*}
	${APPLY} -0 -d who 1 2 3 4 5 > ${*}.res
	@cmp -s ${.CURDIR}/${*}.out ${*}.res || \
	    (echo "XXX ${*} failed" && false)

t4:
	@echo ${*}
	${APPLY} -2 -d cmp a1 b1 a2 b2 a3 b3 > ${*}.res
	@cmp -s ${.CURDIR}/${*}.out ${*}.res || \
	    (echo "XXX ${*} failed" && false)

t5:
	@echo ${*}
	${APPLY} -a "&" "echo &2 &1" hello world > ${*}.res
	@cmp -s ${.CURDIR}/${*}.out ${*}.res || \
	    (echo "XXX ${*} failed" && false)

t6:
	@echo ${*}
	${APPLY} -a " " -d "2to3  1" test.py > ${*}.res
	@cmp -s ${.CURDIR}/${*}.out ${*}.res || \
	    (echo "XXX ${*} failed" && false)

.include <bsd.regress.mk>
