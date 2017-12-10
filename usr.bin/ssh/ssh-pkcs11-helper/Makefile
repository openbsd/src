#	$OpenBSD: Makefile,v 1.3 2017/12/10 19:37:57 deraadt Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-pkcs11-helper.c ssh-pkcs11.c
SRCS+=	addrmatch.c atomicio.c authfile.c bitmap.c blocks.c bufaux.c buffer.c \
	chacha.c cipher-chachapoly.c cipher.c compat.c digest-openssl.c \
	ed25519.c fatal.c fe25519.c ge25519.c hash.c key.c krl.c log.c match.c \
	misc.c poly1305.c readpass.c sc25519.c ssh-dss.c ssh-ecdsa.c \
	ssh-ed25519.c ssh-rsa.c sshbuf-getput-basic.c sshbuf-getput-crypto.c \
	sshbuf-misc.c sshbuf.c ssherr.c sshkey.c uidswap.c verify.c xmalloc.c

PROG=	ssh-pkcs11-helper
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec
MAN=	ssh-pkcs11-helper.8

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}

