#	$OpenBSD: Makefile,v 1.5 2004/06/17 19:17:48 henning Exp $

.PATH:		${.CURDIR}/..

PROG=	ntpd
SRCS=	ntpd.c buffer.c log.c imsg.c ntp.c ntp_msg.c parse.y config.c \
	server.c client.c
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
MAN=	ntpd.8

.include <bsd.prog.mk>
