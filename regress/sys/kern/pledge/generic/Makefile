#	$OpenBSD: Makefile,v 1.10 2020/04/01 15:26:53 claudio Exp $
PROG=	generic
SRCS=	main.c manager.c test_stdio.c test_tty.c
NOMAN=	yes

LDADD+=		-lutil
CFLAGS+=	-Wall -Werror

REGRESS_TARGETS+=	test_normal

test_normal: ${PROG}
	ulimit -c unlimited && \
	./${PROG} | diff -I OpenBSD -u ${.CURDIR}/tests.out -

.include <bsd.regress.mk>
