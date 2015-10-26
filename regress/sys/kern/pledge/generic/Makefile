#	$OpenBSD: Makefile,v 1.4 2015/10/26 18:52:19 semarie Exp $
PROG=	generic
SRCS+=	main.c manager.c test_stdio.c test_tty.c
NOMAN=	yes

CFLAGS+=	-Wall -Werror

REGRESS_TARGETS+=	test_normal test_systrace
REGRESS_SKIP_TARGETS+=	test_systrace

test_normal: ${PROG}
	./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

test_systrace: ${PROG}
	systrace -A ./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

.include <bsd.regress.mk>
