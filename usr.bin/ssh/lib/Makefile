#	$OpenBSD: Makefile,v 1.47 2004/05/09 01:21:50 djm Exp $

.PATH:		${.CURDIR}/..

LIB=	ssh
SRCS=	authfd.c authfile.c bufaux.c buffer.c canohost.c channels.c \
	cipher.c cipher-3des1.c cipher-aes.c cipher-bf1.c cipher-ctr.c \
	cleanup.c compat.c compress.c crc32.c deattack.c fatal.c \
	hostfile.c log.c match.c nchan.c packet.c readpass.c \
	rsa.c tildexpand.c ttymodes.c xmalloc.c atomicio.c \
	key.c dispatch.c kex.c mac.c uuencode.c misc.c \
	rijndael.c ssh-dss.c ssh-rsa.c dh.c kexdh.c kexgex.c \
	kexdhc.c kexgexc.c scard.c msg.c progressmeter.c dns.c

DEBUGLIBS= no
NOPROFILE= yes
NOPIC=	yes

install:
	@echo -n

.include <bsd.own.mk>

.if (${KERBEROS5:L} == "yes")
CFLAGS+= -DKRB5 -I${DESTDIR}/usr/include/kerberosV

SRCS+= gss-genr.c
CFLAGS+= -DGSSAPI
.endif # KERBEROS5

.include <bsd.lib.mk>
