
PROG=	ssleay

BINOWN= root
BINGRP= bin
BINMODE= 555
BINDIR= /usr/sbin
LDADD=	-lssl -lcrypto
NOMAN= not yet kiddies

SSLEAYDIST= src

SSLEAY_SRC= ${.CURDIR}/../${SSLEAYDIST}/apps

.if ${MACHINE_ARCH} == "i386"
CFLAGS+= -DL_ENDIAN -DBN_ASM
.else
.if ${MACHINE_ARCH} == "alpha"
# no ENDIAN stuff defined for alpha
.else
CFLAGS+= -DB_ENDIAN
.endif
.endif

CFLAGS+= -DMONOLITH -DNO_IDEA -DTERMIOS -DANSI_SOURCE 
CFLAGS+= -I/usr/include/ssl

SRCS=	verify.c asn1pars.c req.c dgst.c dh.c enc.c gendh.c errstr.c ca.c \
	pkcs7.c crl2p7.c crl.c \
	rsa.c dsa.c dsaparam.c \
	x509.c genrsa.c s_server.c s_client.c speed.c \
	s_time.c apps.c s_cb.c s_socket.c version.c sess_id.c \
	ciphers.c ssleay.c

.PATH:	${SSLEAY_SRC}

.include<bsd.prog.mk>

