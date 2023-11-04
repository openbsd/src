#	$OpenBSD: Makefile,v 1.24 2023/11/04 09:38:47 martijn Exp $

PROG=		snmpd
MAN=		snmpd.8 snmpd.conf.5
SRCS=		parse.y log.c snmpe.c application.c application_legacy.c \
		    application_blocklist.c application_internal.c \
		    application_agentx.c ax.c \
		    mps.c trap.c smi.c snmpd.c \
		    proc.c usm.c traphandler.c util.c

LDADD=		-levent -lutil -lcrypto
DPADD=		${LIBEVENT} ${LIBUTIL} ${LIBCRYPTO}
CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
YFLAGS=

.include <bsd.prog.mk>
