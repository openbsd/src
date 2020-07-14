#	$OpenBSD: Makefile,v 1.3 2020/07/14 18:33:00 jsing Exp $

PROGS =	verify x509attribute x509name
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Wall -Werror

REGRESS_TARGETS=regress-verify regress-x509attribute regress-x509name
CLEANFILES+=	x509name.result

regress-verify: verify
	./verify ${.CURDIR}/../certs

regress-x509attribute: x509attribute
	./x509attribute

regress-x509name: x509name
	./x509name > x509name.result
	diff -u ${.CURDIR}/x509name.expected x509name.result

.include <bsd.regress.mk>
