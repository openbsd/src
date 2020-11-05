#	$OpenBSD: Makefile,v 1.4 2020/11/05 20:48:05 bluhm Exp $

PROG=		ocsp_test
LDADD=		-lcrypto -lssl
DPADD=		${LIBCRYPTO} ${LIBSSL}
WARNINGS=	Yes
LDFLAGS+=	-lcrypto -lssl
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror

SERVERS=	www.amazon.com \
		www.cloudflare.com

REGRESS_TARGETS =

.for s in ${SERVERS}
REGRESS_TARGETS +=	run-domain-$s
run-domain-$s: ${PROG}
	@echo '======== $@ ========'
	if ping -n -c 1 -w 1 $s; then \
	    ./${PROG} $s 443; else \
	    echo server $s not reachable; echo SKIPPED; fi
.endfor

.include <bsd.regress.mk>
