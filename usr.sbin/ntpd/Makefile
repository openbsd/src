#	$OpenBSD: Makefile,v 1.7 2004/07/04 11:01:49 alexander Exp $

.PATH:		${.CURDIR}/..

PROG=	ntpd
SRCS=	ntpd.c buffer.c log.c imsg.c ntp.c ntp_msg.c parse.y config.c \
	server.c client.c util.c
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
MAN=	ntpd.8 ntpd.conf.5

.include <bsd.prog.mk>
