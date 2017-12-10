#	$OpenBSD: Makefile,v 1.24 2017/12/10 19:37:57 deraadt Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keygen.c moduli.c
SRCS+=	addrmatch.c atomicio.c authfd.c authfile.c bitmap.c blocks.c chacha.c \
	cipher-chachapoly.c cipher.c cleanup.c digest-openssl.c dns.c \
	ed25519.c fatal.c fe25519.c ge25519.c hash.c hmac.c hostfile.c krl.c \
	log.c match.c misc.c poly1305.c readpass.c sc25519.c ssh-dss.c \
	ssh-ecdsa.c ssh-ed25519.c ssh-pkcs11.c ssh-rsa.c sshbuf-getput-basic.c \
	sshbuf-getput-crypto.c sshbuf-misc.c sshbuf.c ssherr.c sshkey.c \
	uidswap.c utf8.c uuencode.c verify.c xmalloc.c

PROG=	ssh-keygen
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}

