#	$OpenBSD: Makefile,v 1.29 2014/05/29 18:15:21 beck Exp $

PROG=	openssl
LDADD=	-lssl -lcrypto
MAN1=	openssl.1

CFLAGS+= -Wall
CFLAGS+= -DOPENSSL_NO_RC5 -DLIBRESSL_INTERNAL

SRCS=	verify.c asn1pars.c req.c dgst.c dh.c enc.c passwd.c gendh.c errstr.c \
	pkcs7.c crl2p7.c crl.c ca.c \
	rsa.c rsautl.c dsa.c dsaparam.c ec.c ecparam.c \
	x509.c genrsa.c gendsa.c genpkey.c s_server.c s_client.c speed.c \
	s_time.c apps.c s_socket.c version.c sess_id.c \
	ciphers.c nseq.c pkcs12.c pkcs8.c pkey.c pkeyparam.c pkeyutl.c \
	spkac.c smime.c cms.c rand.c engine.c ocsp.c prime.c ts.c \
	openssl.c s_cb.c dhparam.c

.PATH:	${.CURDIR}/../../lib/libssl/src/apps

.include <bsd.prog.mk>
