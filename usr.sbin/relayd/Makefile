#	$OpenBSD: Makefile,v 1.32 2017/07/03 22:21:47 espie Exp $

PROG=		relayd
SRCS=		parse.y
SRCS+=		agentx.c ca.c carp.c check_icmp.c check_script.c \
		check_tcp.c check_tls.c config.c control.c hce.c log.c \
		name2id.c pfe.c pfe_filter.c pfe_route.c proc.c \
		relay.c relay_http.c relay_udp.c relayd.c \
		shuffle.c snmp.c ssl.c util.c
MAN=		relayd.8 relayd.conf.5

LDADD=		-levent -ltls -lssl -lcrypto -lutil
DPADD=		${LIBEVENT} ${LIBSSL} ${LIBCRYPTO} ${LIBUTIL}
CFLAGS+=	-Wall -I${.CURDIR} -I${.CURDIR}/../snmpd
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith
CFLAGS+=	-Wsign-compare -Wcast-qual
YFLAGS=

.include <bsd.prog.mk>
