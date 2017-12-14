#	$OpenBSD: Makefile,v 1.12 2017/12/14 21:07:39 naddy Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keysign.c readconf.c

SRCS+=	addrmatch.c atomicio.c authfile.c bitmap.c canohost.c \
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
