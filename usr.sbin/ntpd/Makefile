#	$OpenBSD: Makefile,v 1.8 2006/05/26 00:33:16 henning Exp $

.PATH:		${.CURDIR}/..

PROG=	ntpd
SRCS=	ntpd.c buffer.c log.c imsg.c ntp.c ntp_msg.c parse.y config.c \
	server.c client.c sensors.c util.c
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
MAN=	ntpd.8 ntpd.conf.5

.include <bsd.prog.mk>
