#	$OpenBSD: Makefile,v 1.93 2017/12/14 21:07:39 naddy Exp $

.PATH:		${.CURDIR}/..

SRCS=	sshd.c auth-rhosts.c auth-passwd.c sshpty.c sshlogin.c servconf.c \
	serverloop.c auth.c auth2.c auth-options.c session.c auth2-chall.c \
	groupaccess.c auth-bsdauth.c auth2-hostbased.c auth2-kbdint.c \
	auth2-none.c auth2-passwd.c auth2-pubkey.c monitor.c monitor_wrap.c \
	sftp-server.c sftp-common.c sandbox-pledge.c

SRCS+=	addrmatch.c atomicio.c authfd.c authfile.c bitmap.c bufaux.c \
	bufbn.c bufec.c buffer.c canohost.c chacha.c channels.c \
	cipher-chachapoly.c cipher.c compat.c crc32.c dh.c digest-openssl.c \
	dispatch.c dns.c ed25519.c fatal.c fe25519.c ge25519.c hash.c hmac.c \
	hostfile.c kex.c kexc25519.c kexc25519c.c kexc25519s.c kexdh.c \
	kexdhc.c kexdhs.c kexecdh.c kexecdhc.c kexecdhs.c kexgex.c kexgexc.c \
	kexgexs.c key.c krl.c log.c mac.c match.c misc.c monitor_fdpass.c \
	msg.c nchan.c opacket.c packet.c poly1305.c progressmeter.c readpass.c \
	sc25519.c smult_curve25519_ref.c ssh-dss.c ssh-ecdsa.c ssh-ed25519.c \
	ssh-pkcs11.c ssh-rsa.c sshbuf-getput-basic.c sshbuf-getput-crypto.c \
	sshbuf-misc.c sshbuf.c ssherr.c sshkey.c ttymodes.c uidswap.c umac.c \
	umac128.c utf8.c uuencode.c verify.c xmalloc.c

PROG=	sshd
BINOWN=	root
BINMODE=555
BINDIR=	/usr/sbin
MAN=	sshd.8 sshd_config.5

.include <bsd.own.mk> # for KERBEROS and AFS

KERBEROS5=no

.if (${KERBEROS5:L} == "yes")
CFLAGS+=-DKRB5 -I${DESTDIR}/usr/include/kerberosV -DGSSAPI
SRCS+=  auth-krb5.c auth2-gss.c gss-serv.c gss-serv-krb5.c
.endif

.include <bsd.prog.mk>

.if (${KERBEROS5:L} == "yes")
LDADD+= -lgssapi -lkrb5 -lasn1
LDADD+= -lwind -lroken -lcom_err -lpthread -lheimbase -lkafs
DPADD+= ${LIBGSSAPI} ${LIBKRB5}
.endif

.if (${OPENSSL:L} == "yes")
LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}
.endif

LDADD+=	-lz
DPADD+=	${LIBZ}
