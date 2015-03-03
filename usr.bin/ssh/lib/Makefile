#	$OpenBSD: Makefile,v 1.83 2015/03/03 21:21:13 djm Exp $

.PATH:		${.CURDIR}/..
.include "${.CURDIR}/../Makefile.inc"

LIB=	ssh

# These are files intended for a standalone libopenssh in the future.
LIB_SRCS= \
	ssh_api.c \
	ssherr.c \
	sshbuf.c \
	sshbuf-getput-basic.c \
	sshbuf-misc.c \
	sshkey.c \
	bitmap.c \
	krl.c

.if (${OPENSSL:L} == "yes")
LIB_SRCS+=	sshbuf-getput-crypto.c digest-openssl.c
.else
LIB_SRCS+=	digest-libc.c
.endif

SRCS=	${LIB_SRCS} \
	authfd.c authfile.c bufaux.c buffer.c canohost.c \
	channels.c cipher.c \
	cleanup.c compat.c crc32.c deattack.c fatal.c \
	hostfile.c log.c match.c nchan.c packet.c opacket.c readpass.c \
	ttymodes.c xmalloc.c atomicio.c \
	key.c dispatch.c kex.c mac.c uidswap.c uuencode.c misc.c \
	msg.c progressmeter.c dns.c \
	monitor_fdpass.c addrmatch.c \
	smult_curve25519_ref.c \
	kexc25519.c kexc25519c.c kexc25519s.c \
	roaming_dummy.c \
	chacha.c poly1305.c cipher-chachapoly.c ssh-ed25519.c hmac.c umac.c

.if (${SSH1:L} == "yes")
SRCS+=	cipher-3des1.c cipher-bf1.c
.endif

.if (${OPENSSL:L} == "yes")
SRCS+=	bufec.c bufbn.c rsa.c \
	ssh-dss.c ssh-rsa.c ssh-ecdsa.c dh.c \
	kexdh.c kexgex.c kexecdh.c \
	kexdhc.c kexgexc.c kexecdhc.c \
	kexdhs.c kexgexs.c kexecdhs.c \
	ssh-pkcs11.c
.else
SRCS+=	rijndael.c cipher-aesctr.c
.endif

# ed25519, from supercop
SRCS+=	sc25519.c ge25519.c fe25519.c ed25519.c verify.c hash.c blocks.c

SRCS+=	umac128.c
CLEANFILES+=   umac128.c
umac128.c: umac.c Makefile
	sed \
	    -e "s/^#define UMAC_OUTPUT_LEN     8/#define UMAC_OUTPUT_LEN 16/" \
	    -e s/umac_new/umac128_new/g \
	    -e s/umac_update/umac128_update/g \
	    -e s/umac_final/umac128_final/g \
	    -e s/umac_delete/umac128_delete/g \
	    < ${.CURDIR}/../umac.c > ${.TARGET}

DEBUGLIBS= no
NOPROFILE= yes
NOPIC=	yes

install:
	@echo -n

.include <bsd.own.mk>

# .if (${KERBEROS5:L} == "yes")
# CFLAGS+= -DKRB5 -I${DESTDIR}/usr/include/kerberosV

# SRCS+= gss-genr.c
# CFLAGS+= -DGSSAPI
# .endif # KERBEROS5

.include <bsd.lib.mk>
