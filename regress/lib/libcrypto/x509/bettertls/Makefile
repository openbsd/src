#	$OpenBSD: Makefile,v 1.6 2024/12/27 08:02:27 tb Exp $

PROGS =		verify

.ifdef EOPENSSL33
LDADD +=	-Wl,-rpath,/usr/local/lib/eopenssl33 -L/usr/local/lib/eopenssl33
CFLAGS +=	-I/usr/local/include/eopenssl33/
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
