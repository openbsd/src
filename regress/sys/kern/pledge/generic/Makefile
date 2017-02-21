#	$OpenBSD: Makefile,v 1.8 2017/02/21 15:46:25 tb Exp $
PROG=	generic
SRCS+=	main.c manager.c test_stdio.c test_tty.c
NOMAN=	yes

LDADD+=		-lutil
CFLAGS+=	-Wall -Werror

REGRESS_TARGETS+=	test_normal

test_normal: ${PROG}
	./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

.include <bsd.regress.mk>
