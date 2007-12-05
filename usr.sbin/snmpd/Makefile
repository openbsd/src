#	$OpenBSD: Makefile,v 1.1 2007/12/05 09:22:44 reyk Exp $

PROG=		snmpd
MAN=		snmpd.8 snmpd.conf.5 ber.3
SRCS=		parse.y ber.c log.c control.c buffer.c imsg.c snmpe.c \
		    mps.c mib.c kroute.c snmpd.c

LDADD=		-levent
DPADD=		${LIBEVENT}
CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CLEANFILES+=	y.tab.h

.include <bsd.prog.mk>
