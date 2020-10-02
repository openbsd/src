#	$OpenBSD: Makefile,v 1.2 2020/10/02 08:07:33 tb Exp $

PROGS= verify 
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-g -DLIBRESSL_INTERNAL -Wundef -Werror

REGRESS_TARGETS=  bettertls-test
CLEANFILES+=	test.out

bettertls-test: bettertls-verify
	${.CURDIR}/check.perl ${.CURDIR}/expected ${.CURDIR}/known ${.OBJDIR}/test.out

bettertls-verify: verify
	./verify ${.CURDIR}/certificates > test.out

.include <bsd.regress.mk>
