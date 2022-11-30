#	$OpenBSD: Makefile,v 1.7 2022/11/30 02:51:05 jsing Exp $

SUBDIR= \
	addsub \
	general \
	mont \
	rand

PROGS+= bn_cmp

LDADD=		-lcrypto
DPADD=		${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Wall -Wundef -Werror

REGRESS_TARGETS += run-bn_cmp
run-bn_cmp: bn_cmp
	./bn_cmp

.include <bsd.regress.mk>
