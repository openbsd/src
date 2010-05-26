#	$OpenBSD: Makefile,v 1.20 2010/05/26 16:44:32 nicm Exp $

PROG=		relayd
SRCS=		parse.y log.c control.c ssl.c ssl_privsep.c \
		relayd.c pfe.c pfe_filter.c pfe_route.c hce.c relay.c \
		relay_udp.c carp.c check_icmp.c check_tcp.c check_script.c \
		name2id.c snmp.c shuffle.c
MAN=		relayd.8 relayd.conf.5

LDADD=		-levent -lssl -lcrypto -lutil
DPADD=		${LIBEVENT} ${LIBSSL} ${LIBCRYPTO} ${LIBUTIL}
CFLAGS+=	-Wall -I${.CURDIR} -I${.CURDIR}/../snmpd
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CLEANFILES+=	y.tab.h

.include <bsd.prog.mk>
