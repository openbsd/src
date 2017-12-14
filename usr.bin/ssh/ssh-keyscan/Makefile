#	$OpenBSD: Makefile,v 1.10 2017/12/14 21:07:39 naddy Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keyscan.c

SRCS+=	addrmatch.c atomicio.c canohost.c chacha.c \
	cipher-chachapoly.c cipher.c cleanup.c compat.c dh.c digest-openssl.c \
	dispatch.c ed25519.c fe25519.c ge25519.c hash.c hmac.c hostfile.c \
	kex.c kexc25519.c kexc25519c.c kexc25519s.c kexdh.c kexdhc.c kexdhs.c \
	kexecdh.c kexecdhc.c kexecdhs.c kexgex.c kexgexc.c kexgexs.c log.c \
	mac.c match.c misc.c packet.c poly1305.c sc25519.c \
	smult_curve25519_ref.c ssh-dss.c ssh-ecdsa.c ssh-ed25519.c ssh-rsa.c \
	ssh_api.c sshbuf-getput-basic.c sshbuf-getput-crypto.c sshbuf-misc.c \
	sshbuf.c ssherr.c sshkey.c uidswap.c umac.c umac128.c verify.c \
	xmalloc.c

PROG=	ssh-keyscan
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+= -lcrypto -lz -lutil
DPADD+= ${LIBCRYPTO} ${LIBZ} ${LIBUTIL}

