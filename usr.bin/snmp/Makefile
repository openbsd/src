#	$OpenBSD: Makefile,v 1.1 2019/08/09 06:17:59 martijn Exp $

PROG=		snmp
SRCS=		mib.c smi.c snmp.c snmpc.c
LDADD+=		-lutil
DPADD+=		${LIBUTIL}

MAN=		snmp.1

CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare

.include <bsd.prog.mk>
