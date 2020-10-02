#	$OpenBSD: Makefile,v 1.4 2020/10/02 10:03:18 tb Exp $

PROGS =		verify 

.ifdef EOPENSSL11
LDADD +=	-Wl,-rpath,/usr/local/lib/eopenssl11 -L/usr/local/lib/eopenssl11
CFLAGS +=	-I/usr/local/include/eopenssl11/
.endif

LDADD +=	-lcrypto
DPADD +=	${LIBCRYPTO}
WARNINGS ?=	Yes
CFLAGS +=	-g -DLIBRESSL_INTERNAL -Wundef -Werror

REGRESS_TARGETS =  bettertls-test
CLEANFILES +=	test.out

bettertls-test: bettertls-verify
	${.CURDIR}/check.perl ${.CURDIR}/expected ${.CURDIR}/known ${.OBJDIR}/test.out

bettertls-verify: verify
	./verify ${.CURDIR}/certificates > test.out

.include <bsd.regress.mk>
