#	$OpenBSD: Makefile,v 1.1 2020/07/15 03:12:42 beck Exp $

PROGS = verify 
LDADD=	-Wl,-Bstatic -lcrypto -Wl,-Bdynamic
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-g -DLIBRESSL_INTERNAL -Wundef -Werror -I$(BSDSRCDIR)/lib/libcrypto/x509

REGRESS_TARGETS=  bettertls-test
CLEANFILES+=	test.out

bettertls-test: bettertls-verify
	${.CURDIR}/check.perl ${.CURDIR}/expected ${.CURDIR}/known ${.OBJDIR}/test.out

bettertls-verify: verify
	./verify ${.CURDIR}/certificates > test.out

.include <bsd.regress.mk>
