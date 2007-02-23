#	$OpenBSD: Makefile,v 1.9 2007/02/23 00:28:06 deraadt Exp $

PROG=		hoststated
SRCS=		parse.y log.c control.c buffer.c imsg.c hoststated.c	\
		ssl.c pfe.c pfe_filter.c hce.c				\
		check_icmp.c check_tcp.c relay.c carp.c
MAN=		hoststated.8 hoststated.conf.5

LDADD=		-levent -lssl -lcrypto
DPADD=		${LIBEVENT} ${LIBSSL} ${LIBCRYPTO}
CFLAGS+=	-Wall -Werror -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CLEANFILES+=	y.tab.h

.include <bsd.prog.mk>
