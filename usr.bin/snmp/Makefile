#	$OpenBSD: Makefile,v 1.3 2019/09/18 09:52:47 martijn Exp $

PROG=		snmp
SRCS=		mib.c smi.c snmp.c snmpc.c usm.c
LDADD+=		-lcrypto -lutil
DPADD+=		${LIBCRYPTO} ${LIBUTIL}

MAN=		snmp.1

CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare

.include <bsd.prog.mk>
