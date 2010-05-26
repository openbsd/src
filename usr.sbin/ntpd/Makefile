#	$OpenBSD: Makefile,v 1.11 2010/05/26 16:44:32 nicm Exp $

PROG=	ntpd
SRCS=	ntpd.c log.c ntp.c ntp_msg.c parse.y config.c \
	server.c client.c sensors.c util.c ntp_dns.c
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-lutil
DPADD+= ${LIBUTIL}
MAN=	ntpd.8 ntpd.conf.5

.include <bsd.prog.mk>
