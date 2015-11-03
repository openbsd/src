#	$OpenBSD: Makefile,v 1.6 2015/11/03 11:03:10 semarie Exp $
PROG=	generic
SRCS+=	main.c manager.c test_stdio.c test_tty.c
NOMAN=	yes

CLEANFILES += systrace.filter systrace.err

CFLAGS+=	-Wall -Werror

REGRESS_TARGETS+=	test_normal test_systrace_1 test_systrace_2

test_normal: ${PROG}
	./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

test_systrace_1: ${PROG} systrace.filter
	systrace -e -a -f systrace.filter ./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

test_systrace_2: ${PROG} systrace.filter
	systrace -E systrace.err -a -f systrace.filter ./${PROG} >/dev/null
	test ! -s systrace.err

systrace.filter: systrace.filter.in
	sed 's,$${OBJDIR},'"$$(readlink -nf ${PWD})," ${.CURDIR}/systrace.filter.in >$@.tmp
	mv $@.tmp $@

.include <bsd.regress.mk>
