#	$OpenBSD: Makefile,v 1.1 2006/12/16 11:45:07 reyk Exp $

PROG=		hostated
SRCS=		parse.y log.c control.c buffer.c imsg.c hostated.c \
		pfe.c pfe_filter.c hce.c check_icmp.c check_tcp.c check_http.c
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
