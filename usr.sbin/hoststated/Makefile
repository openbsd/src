#	$OpenBSD: Makefile,v 1.3 2007/01/09 00:45:32 deraadt Exp $

PROG=		hoststated
SRCS=		parse.y log.c control.c buffer.c imsg.c hoststated.c 	\
		pfe.c pfe_filter.c hce.c 				\
		check_icmp.c check_tcp.c check_http.c check_send_expect.c
MAN=		hostated.8 hostated.conf.5

LDADD=		-levent
DPADD=		${LIBEVENT}
CFLAGS+=	-Wall -Werror -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CLEANFILES+=	y.tab.h

.include <bsd.prog.mk>
