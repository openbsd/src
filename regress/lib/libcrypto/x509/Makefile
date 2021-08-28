#	$OpenBSD: Makefile,v 1.5 2021/08/28 15:13:50 beck Exp $

PROGS =	constraints verify x509attribute x509name
LDADD=	-Wl,-Bstatic -lcrypto -Wl,-Bdynamic
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Werror -I$(BSDSRCDIR)/lib/libcrypto/x509

SUBDIR += bettertls

REGRESS_TARGETS=regress-constraints regress-verify regress-x509attribute regress-x509name
CLEANFILES+=	x509name.result

.if make(clean) || make(cleandir)
.BEGIN:
	 rm -rf ${.OBJDIR}/[0-9]*
.endif

regress-verify: verify
	perl ${.CURDIR}/make-dir-roots.pl ${.CURDIR}/../certs .
	./verify ${.CURDIR}/../certs

regress-constraints: constraints
	./constraints

regress-x509attribute: x509attribute
	./x509attribute

regress-x509name: x509name
	./x509name > x509name.result
	diff -u ${.CURDIR}/x509name.expected x509name.result

.include <bsd.regress.mk>
