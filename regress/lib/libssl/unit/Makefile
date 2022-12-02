#	$OpenBSD: Makefile,v 1.14 2022/12/02 01:15:11 tb Exp $

PROGS += cipher_list
PROGS += ssl_get_shared_ciphers
PROGS += ssl_methods
PROGS += ssl_set_alpn_protos
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
