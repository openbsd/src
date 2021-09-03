#	$OpenBSD: Makefile,v 1.8 2021/09/03 08:58:53 beck Exp $

PROGS =	constraints verify x509attribute x509name callback
LDADD=	-Wl,-Bstatic -lcrypto -Wl,-Bdynamic
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Werror -I$(BSDSRCDIR)/lib/libcrypto/x509

SUBDIR += bettertls

REGRESS_TARGETS += regress-verify
REGRESS_TARGETS += regress-constraints
REGRESS_TARGETS += regress-x509attribute
REGRESS_TARGETS += regress-x509name
REGRESS_TARGETS += regress-callback

CLEANFILES+=	x509name.result callbackout

.if make(clean) || make(cleandir)
. if ${.OBJDIR} != ${.CURDIR}
.BEGIN:
	 rm -rf [0-9]*
. endif
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

regress-callback: callback
	./callback ${.CURDIR}/../certs
	perl ${.CURDIR}/callback.pl callback.out

.include <bsd.regress.mk>
