
PROG=	openssl

BINOWN= root
BINGRP= bin
BINMODE= 555
BINDIR= /usr/sbin
LDADD=	-lssl -lcrypto
NOMAN= not yet kiddies

SSLEAYDIST= lib/libssl/src

SSLEAY_SRC= ${.CURDIR}/../../${SSLEAYDIST}/apps

.if ${MACHINE_ARCH} == "i386"
CFLAGS+= -g -DL_ENDIAN -DBN_ASM
.else
.if ${MACHINE_ARCH} == "arc"
CFLAGS+= -DL_ENDIAN
.else
.if ${MACHINE_ARCH} == "pmax"
CFLAGS+= -DL_ENDIAN
.else
.if ${MACHINE_ARCH} == "alpha"
# no ENDIAN stuff defined for alpha
.else
CFLAGS+= -DB_ENDIAN
.endif
.endif
.endif
.endif 

CFLAGS+= -DMONOLITH -DNO_IDEA -DTERMIOS -DANSI_SOURCE 
CFLAGS+= -I/usr/include/ssl

#SRCS=	verify.c asn1pars.c req.c dgst.c dh.c enc.c gendh.c errstr.c ca.c \
#	pkcs7.c crl2p7.c crl.c \
#	rsa.c dsa.c dsaparam.c \
#	x509.c genrsa.c gendsa.c s_server.c s_client.c speed.c \
#	s_time.c apps.c s_cb.c s_socket.c version.c sess_id.c \
#	ciphers.c nseq.c pkcs12.c pkcs8.c openssl.c
SRCS=	verify.c asn1pars.c req.c dgst.c dh.c enc.c passwd.c gendh.c errstr.c\
	pkcs7.c crl2p7.c crl.c ca.c \
	rsa.c dsa.c dsaparam.c \
	x509.c genrsa.c gendsa.c s_server.c s_client.c speed.c \
	s_time.c apps.c s_cb.c s_socket.c version.c sess_id.c \
	app_rand.c ciphers.c nseq.c pkcs12.c pkcs8.c spkac.c smime.c \
	dhparam.c openssl.c

.PATH:	${SSLEAY_SRC}

.include<bsd.prog.mk>

