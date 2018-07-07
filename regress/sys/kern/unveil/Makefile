#	$OpenBSD: Makefile,v 1.2 2018/07/07 23:16:24 beck Exp $
PROG=	unveil_test
SRCS+=	syscalls.c
NOMAN=	yes

LDADD+=		-lutil
CFLAGS+=	-Wall

REGRESS_TARGETS+=	test_normal

test_normal: ${PROG}
	./${PROG} 

.include <bsd.regress.mk>
