#	$OpenBSD: Makefile,v 1.22 2013/08/06 19:05:57 miod Exp $

PROG=	openssl
LDADD=	-lssl -lcrypto
MAN1=	openssl.1

SSLEAYDIST= lib/libssl/src

SSLEAY_SRC= ${.CURDIR}/../../${SSLEAYDIST}/apps
SSLEAY_SRC_TOP= ${.CURDIR}/../../${SSLEAYDIST}

CFLAGS+= -DMONOLITH -DTERMIOS -DANSI_SOURCE -DOPENSSL_NO_RC5
CFLAGS+= -DOPENSSL_NO_SSL2
CFLAGS+= -I${SSLEAY_SRC_TOP}

.if ${MACHINE} == "vax"
LDFLAGS+=-Wl,--no-keep-memory
.endif

SRCS=	verify.c asn1pars.c req.c dgst.c dh.c enc.c passwd.c gendh.c errstr.c \
	pkcs7.c crl2p7.c crl.c ca.c \
	rsa.c rsautl.c dsa.c dsaparam.c ec.c ecparam.c \
	x509.c genrsa.c gendsa.c genpkey.c s_server.c s_client.c speed.c \
	s_time.c apps.c s_socket.c app_rand.c version.c sess_id.c \
	ciphers.c nseq.c pkcs12.c pkcs8.c pkey.c pkeyparam.c pkeyutl.c \
	spkac.c smime.c cms.c rand.c engine.c ocsp.c prime.c ts.c \
	openssl.c s_cb.c dhparam.c

.PATH:	${SSLEAY_SRC}

.include <bsd.prog.mk>
