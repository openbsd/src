#	$OpenBSD: Makefile,v 1.18 2008/09/06 12:21:52 djm Exp $

PROG=	openssl
LDADD=	-lssl -lcrypto
MAN1=	openssl.1

SSLEAYDIST= lib/libssl/src

SSLEAY_SRC= ${.CURDIR}/../../${SSLEAYDIST}/apps
SSLEAY_SRC_TOP= ${.CURDIR}/../../${SSLEAYDIST}

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "vax" || \
    ${MACHINE_ARCH} == "amd64"

CFLAGS+= -DL_ENDIAN
.else
.if ${MACHINE_ARCH} == "alpha"
# no ENDIAN stuff defined for alpha
.else
CFLAGS+= -DB_ENDIAN
.endif
.endif

CFLAGS+= -DMONOLITH -DOPENSSL_NO_IDEA -DTERMIOS -DANSI_SOURCE -DOPENSSL_NO_RC5
CFLAGS+= -DOPENSSL_NO_MDC2
CFLAGS+= -I${SSLEAY_SRC_TOP}

SRCS=	verify.c asn1pars.c req.c dgst.c dh.c enc.c passwd.c gendh.c errstr.c\
	pkcs7.c crl2p7.c crl.c ca.c \
	rsa.c dsa.c dsaparam.c \
	x509.c genrsa.c gendsa.c s_server.c s_client.c speed.c \
	s_time.c apps.c s_cb.c s_socket.c version.c sess_id.c \
	app_rand.c ciphers.c nseq.c pkcs12.c pkcs8.c spkac.c smime.c \
	dhparam.c openssl.c rand.c rsautl.c engine.c ocsp.c prime.c \
	cms.c ec.c ecparam.c

.PATH:	${SSLEAY_SRC}

.include <bsd.prog.mk>
