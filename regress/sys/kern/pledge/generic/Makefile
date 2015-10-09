#	$OpenBSD: Makefile,v 1.3 2015/10/09 11:38:39 semarie Exp $
PROG=	generic
SRCS+=	main.c manager.c test_tty.c
NOMAN=	yes

CFLAGS+=	-Wall -Werror

REGRESS_TARGETS+=	test_normal test_systrace
REGRESS_SKIP_TARGETS+=	test_systrace

test_normal: ${PROG}
	./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

test_systrace: ${PROG}
	systrace -A ./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

.include <bsd.regress.mk>
