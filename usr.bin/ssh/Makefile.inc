#	$OpenBSD: Makefile.inc,v 1.104 2025/10/30 23:19:33 djm Exp $

.include <bsd.own.mk>

CFLAGS+=	-I${.CURDIR}/..
.if ${MACHINE} != "hppa"
CFLAGS+=	-fstack-protector-all
.endif
.if ${MACHINE} == "amd64"
CFLAGS+=	-fret-clean
.endif

CDIAGFLAGS=	-Wall
CDIAGFLAGS+=	-Wextra
CDIAGFLAGS+=	-Wformat-security
CDIAGFLAGS+=	-Wmissing-declarations
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wold-style-definition
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wshadow
CDIAGFLAGS+=	-Wstrict-aliasing=2
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wunused
CDIAGFLAGS+=	-Wno-unused-parameter # Lots of these in protocol handlers.
.if ${COMPILER_VERSION:L} == "clang"
CDIAGFLAGS+=	-Widiomatic-parentheses -Wparentheses
.endif
CDIAGFLAGS+=	-Werror

#DEBUG=-g
#INSTALL_STRIP=

WARNINGS=yes

OPENSSL?=	yes
ZLIB?=		yes

.if (${OPENSSL:L} == "yes")
CFLAGS+=	-DWITH_OPENSSL
.endif

.if (${ZLIB:L} == "yes")
CFLAGS+=	-DWITH_ZLIB
.endif

CFLAGS+=	-DENABLE_PKCS11
.ifndef NOPIC
CFLAGS+=	-DHAVE_DLOPEN
.endif

SRCS_BASE+=	sshbuf.c
SRCS_BASE+=	sshbuf-getput-basic.c
SRCS_BASE+=	sshbuf-misc.c
SRCS_BASE+=	ssherr.c
SRCS_BASE+=	log.c
SRCS_BASE+=	xmalloc.c
SRCS_BASE+=	misc.c
SRCS_BASE+=	addr.c
SRCS_BASE+=	addrmatch.c
SRCS_BASE+=	match.c

.if (${OPENSSL:L} == "yes")
SRCS_KEX+=	dh.c
SRCS_KEX+=	kexdh.c
SRCS_KEX+=	kexecdh.c
SRCS_KEX+=	kexgex.c
SRCS_KEXC+=	kexgexc.c
SRCS_KEXS+=	kexgexs.c
.endif
SRCS_KEX+=	kexc25519.c
SRCS_KEX+=	smult_curve25519_ref.c
SRCS_KEX+=	kexgen.c
SRCS_KEX+=	kexsntrup761x25519.c
SRCS_KEX+=	sntrup761.c
SRCS_KEX+=	kexmlkem768x25519.c

SRCS_KEY+=	sshkey.c
SRCS_KEY+=	cipher.c
SRCS_KEY+=	chacha.c
SRCS_KEY+=	poly1305.c
.if (${OPENSSL:L} == "yes")
SRCS_KEY+=	ssh-ecdsa.c
SRCS_KEY+=	ssh-ecdsa-sk.c
SRCS_KEY+=	ssh-rsa.c
SRCS_KEY+=	sshbuf-getput-crypto.c
SRCS_KEY+=	digest-openssl.c
SRCS_KEY+=	cipher-chachapoly-libcrypto.c
.else
SRCS_KEY+=	cipher-aesctr.c
SRCS_KEY+=	rijndael.c
SRCS_KEY+=	digest-libc.c
SRCS_KEY+=	cipher-chachapoly.c
.endif
SRCS_KEY+=	ssh-ed25519.c
SRCS_KEY+=	ssh-ed25519-sk.c

.if (${OPENSSL:L} == "yes")
SRCS_KEY+=	ed25519-openssl.c
.else
# ed25519, from supercop
SRCS_KEY+=	ed25519.c
.endif

SRCS_KEYP+=	authfile.c
SRCS_KEYP+=	sshbuf-io.c
SRCS_KEYP+=	atomicio.c

SRCS_KRL+=	bitmap.c
SRCS_KRL+=	krl.c

SRCS_MAC+=	mac.c
SRCS_MAC+=	hmac.c
SRCS_MAC+=	umac.c
SRCS_MAC+=	umac128.c

SRCS_PKT+=	canohost.c
SRCS_PKT+=	dispatch.c
SRCS_PKT+=	kex.c
SRCS_PKT+=	kex-names.c
SRCS_PKT+=	packet.c
SRCS_PKT+=	${SRCS_MAC}

SRCS_PROT+=	channels.c
SRCS_PROT+=	monitor_fdpass.c
SRCS_PROT+=	nchan.c
SRCS_PROT+=	ttymodes.c

SRCS_PKCS11+=		ssh-pkcs11.c
SRCS_PKCS11_CLIENT+=	ssh-pkcs11-client.c

.if (${OPENSSL:L} == "yes")
SRCS_MODULI+=		moduli.c
.else
SRCS_MODULI+=
.endif

SRCS_SK=		ssh-sk.c
SRCS_SK+=		sk-usbhid.c
SRCS_SK_CLIENT=		ssh-sk-client.c
SRCS_SK_CLIENT+=	msg.c

.include <bsd.obj.mk>
