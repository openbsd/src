#	$OpenBSD: Makefile,v 1.7 2016/04/25 19:30:10 tedu Exp $
PROG=	generic
SRCS+=	main.c manager.c test_stdio.c test_tty.c
NOMAN=	yes

CFLAGS+=	-Wall -Werror

REGRESS_TARGETS+=	test_normal

test_normal: ${PROG}
	./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

.include <bsd.regress.mk>
