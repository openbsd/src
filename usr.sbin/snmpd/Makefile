#	$OpenBSD: Makefile,v 1.28 2026/10/30 16:40:40 martijn Exp $

PROG=		snmpd
MAN=		snmpd.8 snmpd.conf.5
SRCS=		mib.y parse.y log.c snmpe.c application.c application_blocklist.c \
		    application_internal.c application_agentx.c ax.c \
		    engine.c \
		    trap.c smi.c snmpd.c storage.c \
		    proc.c usm.c traphandler.c util.c

LDADD=		-levent -lutil -lcrypto
DPADD=		${LIBEVENT} ${LIBUTIL} ${LIBCRYPTO}
CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
YFLAGS=

mib.c:		mib.y
	${YACC.y} -pmib -o ${.TARGET} ${.IMPSRC}


.include <bsd.prog.mk>
