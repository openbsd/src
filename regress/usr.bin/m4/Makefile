#	$OpenBSD: Makefile,v 1.19 2004/11/07 10:25:59 espie Exp $

FIBOMAX=25
M4=m4

.SUFFIXES: .m4 .m4.uu
CLEANFILES+= ff_after_dnl.m4 strangequotes.m4 fibo.out

REGRESS_TARGETS= test-ff_after_dnl test-m4wrap test-m4wrap2 test-fibo \
    test-patterns trip test-strangequotes test-redef test-quotes \
    test-weird test-args test-esyscmd test-eval test-gnupatterns

test-ff_after_dnl: ff_after_dnl.m4
	${M4} ff_after_dnl.m4 | diff - ${.CURDIR}/ff_after_dnl.out

.m4.uu.m4:
	uudecode $<

test-m4wrap:
	${M4} ${.CURDIR}/m4wrap.m4 | diff - ${.CURDIR}/m4wrap.out

test-m4wrap2:
	test `${M4} ${.CURDIR}/m4wrap2.m4 | wc -c` == 0

test-patterns:
	${M4} ${.CURDIR}/patterns.m4 | diff - ${.CURDIR}/patterns.out

test-gnupatterns:
	${M4} -g ${.CURDIR}/gnupatterns.m4 | diff - ${.CURDIR}/gnupatterns.out

fibo.out:
	perl ${.CURDIR}/fibo.pl ${FIBOMAX} >$@

test-fibo: fibo.out
	${M4} -DN=${FIBOMAX} ${.CURDIR}/fibo.m4| diff - fibo.out

trip:
	echo "undefine(\`errprint')"|MALLOC_OPTIONS=A ${M4} -Derrprint=a >/dev/null

test-quotes:
	${M4} ${.CURDIR}/quotes.m4 2>&1|	\
		sed -e 's,\( *\).*/quotes.m4,\1quotes.m4,' | \
		diff - ${.CURDIR}/quotes.out
	${M4} -g ${.CURDIR}/quotes.m4 2>&1|diff - ${.CURDIR}/quotes.gnu.out

test-strangequotes: strangequotes.m4
	${M4} strangequotes.m4| diff - ${.CURDIR}/strangequotes.out

test-redef:
	${M4} ${.CURDIR}/redef.m4 | diff - ${.CURDIR}/redef.out

test-weird:
	test `${M4} ${.CURDIR}/weird,name.m4 | wc -c` == 0

test-args:
	${M4} ${.CURDIR}/args.m4 | diff - ${.CURDIR}/args.out

test-esyscmd:
	${M4} ${.CURDIR}/esyscmd.m4 | diff -u - ${.CURDIR}/esyscmd.out

test-eval:
	${M4} ${.CURDIR}/eval.m4 | diff -u - ${.CURDIR}/eval.out

.PHONY:	${REGRESS_TARGETS}

.include <bsd.regress.mk>
