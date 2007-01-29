#	$OpenBSD: Makefile,v 1.5 2007/01/29 14:23:31 pyr Exp $

PROG=		hoststated
SRCS=		parse.y log.c control.c buffer.c imsg.c hoststated.c 	\
		ssl.c pfe.c pfe_filter.c hce.c 				\
		check_icmp.c check_tcp.c check_http.c check_send_expect.c
MAN=		hoststated.8 hoststated.conf.5

LDADD=		-levent -lcrypto -lssl
DPADD=		${LIBEVENT} ${LIBCRYPTO} ${LIBSSL}
CFLAGS+=	-Wall -Werror -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CLEANFILES+=	y.tab.h

.include <bsd.prog.mk>
