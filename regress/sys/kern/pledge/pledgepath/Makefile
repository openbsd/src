#	$OpenBSD: Makefile,v 1.2 2018/04/25 11:55:18 beck Exp $
PROG=	pledgepath_test
SRCS+=	syscalls.c
NOMAN=	yes

LDADD+=		-lutil
CFLAGS+=	-Wall

REGRESS_TARGETS+=	test_normal

test_normal: ${PROG}
	./${PROG} 

.include <bsd.regress.mk>
