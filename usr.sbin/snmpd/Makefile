#	$OpenBSD: Makefile,v 1.14 2014/11/19 10:19:00 blambert Exp $

PROG=		snmpd
MAN=		snmpd.8 snmpd.conf.5
SRCS=		parse.y ber.c log.c control.c snmpe.c \
		    mps.c trap.c mib.c smi.c kroute.c snmpd.c timer.c \
		    pf.c proc.c usm.c agentx.c traphandler.c util.c

LDADD=		-levent -lutil -lkvm -lcrypto
DPADD=		${LIBEVENT} ${LIBUTIL}
CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CLEANFILES+=	y.tab.h

.include <bsd.prog.mk>
