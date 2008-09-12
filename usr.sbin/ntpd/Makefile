#	$OpenBSD: Makefile,v 1.9 2008/09/12 10:46:09 henning Exp $

.PATH:		${.CURDIR}/..

PROG=	ntpd
SRCS=	ntpd.c buffer.c log.c imsg.c ntp.c ntp_msg.c parse.y config.c \
	server.c client.c sensors.c util.c ntp_dns.c
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
MAN=	ntpd.8 ntpd.conf.5

.include <bsd.prog.mk>
