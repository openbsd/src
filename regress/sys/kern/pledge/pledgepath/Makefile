#	$OpenBSD: Makefile,v 1.1 2017/11/27 18:10:30 beck Exp $
PROG=	pledgepath_test
SRCS+=	syscalls.c
NOMAN=	yes

LDADD+=		-lutil
CFLAGS+=	-Wall -Werror

REGRESS_TARGETS+=	test_normal

test_normal: ${PROG}
	./${PROG} 

.include <bsd.regress.mk>
