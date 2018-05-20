#	$OpenBSD: Makefile,v 1.9 2017/10/12 15:07:53 bluhm Exp $
PROG=	generic
SRCS=	main.c manager.c test_stdio.c test_tty.c
NOMAN=	yes

LDADD+=		-lutil
CFLAGS+=	-Wall -Werror

REGRESS_TARGETS+=	test_normal

test_normal: ${PROG}
	./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

.include <bsd.regress.mk>
