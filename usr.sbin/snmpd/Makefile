#	$OpenBSD: Makefile,v 1.10 2013/10/17 08:42:43 reyk Exp $

PROG=		snmpd
MAN=		snmpd.8 snmpd.conf.5
SRCS=		parse.y ber.c log.c control.c snmpe.c \
		    mps.c trap.c mib.c smi.c kroute.c snmpd.c timer.c \
		    pf.c proc.c usm.c

LDADD=		-levent -lutil -lkvm -lcrypto
DPADD=		${LIBEVENT} ${LIBUTIL}
CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CLEANFILES+=	y.tab.h

.include <bsd.prog.mk>
