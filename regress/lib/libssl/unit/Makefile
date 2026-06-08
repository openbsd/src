#	$OpenBSD: Makefile,v 1.19 2026/06/08 11:53:51 tb Exp $

PROGS += cipher_list
PROGS += ssl_get_shared_ciphers
PROGS += ssl_kex
PROGS += ssl_methods
PROGS += ssl_set_alpn_protos
PROGS += ssl_verify_param
PROGS += ssl_versions
PROGS += tls_ext_alpn
PROGS += tls_prf

WARNINGS=	Yes
LDADD =		${SSL_INT} -lcrypto
DPADD =		${LIBSSL} ${LIBCRYPTO}
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror
CFLAGS+=	-DCERTSDIR=\"${.CURDIR}/../certs\"
CFLAGS+=	-I${.CURDIR}/../../../../lib/libssl

.include <bsd.regress.mk>
