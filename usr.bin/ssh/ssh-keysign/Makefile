#	$OpenBSD: Makefile,v 1.10 2017/12/10 19:37:57 deraadt Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keysign.c readconf.c

umac128.c: umac.c
	sed \
	    -e "s/^#define UMAC_OUTPUT_LEN     8/#define UMAC_OUTPUT_LEN 16/" \
	    -e s/umac_new/umac128_new/g \
	    -e s/umac_update/umac128_update/g \
	    -e s/umac_final/umac128_final/g \
	    -e s/umac_delete/umac128_delete/g \
	    < ${.CURDIR}/../umac.c > ${.TARGET}

SRCS+=	addrmatch.c atomicio.c authfile.c bitmap.c blocks.c canohost.c \
	chacha.c cipher-chachapoly.c cipher.c cleanup.c digest-openssl.c \
	dispatch.c ed25519.c fatal.c fe25519.c ge25519.c hash.c hmac.c kex.c \
	krl.c log.c mac.c match.c misc.c msg.c packet.c poly1305.c sc25519.c \
	ssh-dss.c ssh-ecdsa.c ssh-ed25519.c ssh-rsa.c sshbuf-getput-basic.c \
	sshbuf-getput-crypto.c sshbuf-misc.c sshbuf.c ssherr.c sshkey.c \
	uidswap.c umac.c umac128.c verify.c xmalloc.c

PROG=	ssh-keysign
BINOWN=	root

BINMODE?=4555

BINDIR=	/usr/libexec
MAN=	ssh-keysign.8

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}
