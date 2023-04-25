#	$OpenBSD: Makefile,v 1.12 2023/04/25 16:11:02 tb Exp $

.include <bsd.own.mk>

PROG=	openssl
LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}

CFLAGS+= -Wall
CFLAGS+= -Wformat
CFLAGS+= -Wformat-security
CFLAGS+= -Wimplicit
CFLAGS+= -Wreturn-type
#CFLAGS+= -Wshadow
CFLAGS+= -Wtrigraphs
CFLAGS+= -Wuninitialized
CFLAGS+= -Wunused
.if ${COMPILER_VERSION:L} == "clang"
CFLAGS+= -Werror
.endif
CFLAGS+= -DLIBRESSL_INTERNAL

SRCS=	apps.c apps_posix.c asn1pars.c ca.c certhash.c ciphers.c cms.c crl.c \
	crl2p7.c dgst.c dh.c dhparam.c dsa.c dsaparam.c ec.c ecparam.c enc.c \
	errstr.c gendh.c gendsa.c genpkey.c genrsa.c ocsp.c \
	openssl.c passwd.c pkcs12.c pkcs7.c pkcs8.c pkey.c pkeyparam.c \
	pkeyutl.c prime.c rand.c req.c rsa.c rsautl.c s_cb.c s_client.c \
	s_server.c s_socket.c s_time.c sess_id.c smime.c speed.c spkac.c ts.c \
	verify.c version.c x509.c

.include <bsd.prog.mk>
