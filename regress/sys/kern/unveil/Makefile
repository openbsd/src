#	$OpenBSD: Makefile,v 1.3 2019/08/06 20:21:36 bluhm Exp $

PROG=	unveil_test
SRCS=	syscalls.c
CFLAGS=	-Wall
NOMAN=	yes
LDADD=	-lutil
DPADD=	${LIBUTIL}

REGRESS_TARGETS =	test_normal
REGRESS_ROOT_TARGETS =	test_normal

test_normal: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>
