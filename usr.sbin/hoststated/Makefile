#	$OpenBSD: Makefile,v 1.12 2007/05/29 17:12:04 reyk Exp $

PROG=		hoststated
SRCS=		parse.y log.c control.c buffer.c imsg.c hoststated.c	\
		ssl.c pfe.c pfe_filter.c hce.c relay.c carp.c 		\
		check_icmp.c check_tcp.c check_script.c
MAN=		hoststated.8 hoststated.conf.5

LDADD=		-levent -lssl -lcrypto
DPADD=		${LIBEVENT} ${LIBSSL} ${LIBCRYPTO}
CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CLEANFILES+=	y.tab.h

.include <bsd.prog.mk>
