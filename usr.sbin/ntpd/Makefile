#	$OpenBSD: Makefile,v 1.15 2015/10/05 17:26:22 deraadt Exp $

PROG=	ntpd
SRCS=	ntpd.c log.c ntp.c ntp_msg.c parse.y config.c \
	server.c client.c sensors.c util.c ntp_dns.c \
	control.c constraint.c
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -fstack-protector-all
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-lutil -ltls -lssl -lcrypto
DPADD+= ${LIBUTIL} ${LIBCRYPTO} ${LIBSSL} ${LIBTLS}
LINKS=	${BINDIR}/ntpd ${BINDIR}/ntpctl
MAN=	ntpd.8 ntpd.conf.5 ntpctl.8

.include <bsd.prog.mk>
