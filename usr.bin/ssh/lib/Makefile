#	$OpenBSD: Makefile,v 1.23 2001/05/30 12:55:14 markus Exp $

.PATH:		${.CURDIR}/..

LIB=	ssh
SRCS=   authfd.c authfile.c bufaux.c buffer.c canohost.c \
	channel.c channel-close.c channel-input.c channel-core.c \
	channel-tcpfwd.c channel-auth.c channel-x11.c \
	cipher.c compat.c compress.c crc32.c deattack.c \
	hostfile.c log.c match.c mpaux.c packet.c readpass.c \
	rsa.c tildexpand.c ttymodes.c uidswap.c xmalloc.c atomicio.c \
	key.c dispatch.c kex.c mac.c uuencode.c misc.c \
	cli.c rijndael.c ssh-dss.c ssh-rsa.c dh.c kexdh.c kexgex.c \

NOPROFILE= yes
NOPIC=	yes

install:
	@echo -n

.include <bsd.own.mk>

.if (${KERBEROS:L} == "yes")
CFLAGS+= -DKRB4 -I${DESTDIR}/usr/include/kerberosIV
.if (${AFS:L} == "yes")
CFLAGS+= -DAFS
SRCS+=  radix.c
.endif # AFS
.endif # KERBEROS

.include <bsd.lib.mk>
