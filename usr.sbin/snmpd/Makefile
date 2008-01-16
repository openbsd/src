#	$OpenBSD: Makefile,v 1.4 2008/01/16 09:42:29 reyk Exp $

PROG=		snmpd
MAN=		snmpd.8 snmpd.conf.5
SRCS=		parse.y ber.c log.c control.c buffer.c imsg.c snmpe.c \
		    mps.c trap.c mib.c smi.c kroute.c snmpd.c

LDADD=		-levent
DPADD=		${LIBEVENT}
CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CLEANFILES+=	y.tab.h

.include <bsd.prog.mk>
