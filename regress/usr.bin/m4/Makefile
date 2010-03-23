#	$OpenBSD: Makefile,v 1.28 2010/03/23 20:11:52 espie Exp $

FIBOMAX=25
M4=m4

.SUFFIXES: .m4 .m4.uu
CLEANFILES+= ff_after_dnl.m4 strangequotes.m4 fibo.out

REGRESS_TARGETS= test-ff_after_dnl test-m4wrap test-m4wrap2 \
    test-m4wrap3 test-gm4wrap3 test-fibo \
    test-patterns trip test-strangequotes test-redef test-quotes \
    test-weird test-args test-args2 test-esyscmd test-eval test-gnupatterns \
    test-gnupatterns2 test-comments test-synch1 test-synch1bis \
    test-gnuformat test-includes test-dumpdef test-gnuprefix \
    test-translit

test-ff_after_dnl: ff_after_dnl.m4
	${M4} ff_after_dnl.m4 | diff - ${.CURDIR}/ff_after_dnl.out

.m4.uu.m4:
	uudecode $<

test-m4wrap:
	${M4} ${.CURDIR}/m4wrap.m4 | diff - ${.CURDIR}/m4wrap.out

test-m4wrap2:
	test `${M4} ${.CURDIR}/m4wrap2.m4 | wc -c` == 0

test-m4wrap3:
	${M4} ${.CURDIR}/m4wrap3.m4 | diff - ${.CURDIR}/m4wrap3.out

test-gm4wrap3:
	${M4} -g ${.CURDIR}/m4wrap3.m4 | diff - ${.CURDIR}/gm4wrap3.out

test-patterns:
	${M4} ${.CURDIR}/patterns.m4 | diff - ${.CURDIR}/patterns.out

test-gnupatterns:
	${M4} -g ${.CURDIR}/gnupatterns.m4 | diff - ${.CURDIR}/gnupatterns.out

test-gnupatterns2:
	${M4} -g ${.CURDIR}/gnupatterns2.m4 | diff - ${.CURDIR}/gnupatterns2.out

test-gnuformat:
	${M4} -g ${.CURDIR}/gnuformat.m4 | diff - ${.CURDIR}/gnuformat.out

test-translit:
	${M4} ${.CURDIR}/translit.m4 | diff - ${.CURDIR}/translit.out

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

test-comments:
	${M4} ${.CURDIR}/comments.m4 | diff - ${.CURDIR}/comments.out
test-strangequotes: strangequotes.m4
	${M4} strangequotes.m4| diff - ${.CURDIR}/strangequotes.out

test-redef:
	${M4} ${.CURDIR}/redef.m4 | diff - ${.CURDIR}/redef.out

test-weird:
	test `${M4} ${.CURDIR}/weird,name.m4 | wc -c` == 0

test-args2:
	${M4} ${.CURDIR}/args2.m4 | diff - ${.CURDIR}/args2.out

test-args:
	${M4} ${.CURDIR}/args.m4 | diff - ${.CURDIR}/args.out

test-includes:
	${M4} -I${.CURDIR} ${.CURDIR}/includes.m4 | \
	    diff - ${.CURDIR}/includes.out

test-esyscmd:
	${M4} ${.CURDIR}/esyscmd.m4 | diff -u - ${.CURDIR}/esyscmd.out

test-eval:
	${M4} ${.CURDIR}/eval.m4 | diff -u - ${.CURDIR}/eval.out

test-synch1:
	${M4} -s ${.CURDIR}/synch1.m4|perl ${.CURDIR}/reconstitute|\
		grep MARK| diff - ${.CURDIR}/synch1.out

test-synch1bis:
	${M4} -s <${.CURDIR}/synch1.m4|perl ${.CURDIR}/reconstitute|\
		grep MARK| diff - ${.CURDIR}/synch1bis.out

test-dumpdef:
	${M4} ${.CURDIR}/dumpdef.m4 2>/dev/null

test-gnuprefix:
	${M4} -P ${.CURDIR}/gnuprefix.m4 2>&1 | \
		diff -u - ${.CURDIR}/gnuprefix.out

.PHONY:	${REGRESS_TARGETS}

.include <bsd.regress.mk>
