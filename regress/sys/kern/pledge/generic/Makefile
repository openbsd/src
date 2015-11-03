#	$OpenBSD: Makefile,v 1.5 2015/11/03 10:08:57 semarie Exp $
PROG=	generic
SRCS+=	main.c manager.c test_stdio.c test_tty.c
NOMAN=	yes

CLEANFILES += systrace.filter

CFLAGS+=	-Wall -Werror

REGRESS_TARGETS+=	test_normal test_systrace

test_normal: ${PROG}
	./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

test_systrace: ${PROG} systrace.filter
	systrace -e -a -f systrace.filter ./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

systrace.filter: systrace.filter.in
	sed 's,$${OBJDIR},'"$$(readlink -nf ${PWD})," ${.CURDIR}/systrace.filter.in >$@.tmp
	mv $@.tmp $@

.include <bsd.regress.mk>
