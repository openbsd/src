#	$OpenBSD: Makefile,v 1.4 2020/09/11 18:34:29 beck Exp $

PROGS =	constraints verify x509attribute x509name
LDADD=	-Wl,-Bstatic -lcrypto -Wl,-Bdynamic
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Werror -I$(BSDSRCDIR)/lib/libcrypto/x509

SUBDIR += bettertls

REGRESS_TARGETS=regress-constraints regress-verify regress-x509attribute regress-x509name
CLEANFILES+=	x509name.result

regress-verify: verify
	./verify ${.CURDIR}/../certs

regress-constraints: constraints
	./constraints

regress-x509attribute: x509attribute
	./x509attribute

regress-x509name: x509name
	./x509name > x509name.result
	diff -u ${.CURDIR}/x509name.expected x509name.result

.include <bsd.regress.mk>
