# $OpenBSD: Makefile,v 1.8 2017/07/06 12:15:23 espie Exp $

PROG=		switchd
MAN=		switchd.8 switchd.conf.5

SRCS=		imsg_util.c log.c packet.c proc.c switch.c timer.c util.c
SRCS+=		switchd.c control.c ofp.c ofp10.c ofp13.c ofp_common.c ofrelay.c
SRCS+=		${.OBJDIR}/ofp_map.c ${.OBJDIR}/ofp10_map.c
SRCS+=		parse.y ofcconn.c

LDADD=		-levent -lutil
DPADD=		${LIBEVENT} ${LIBUTIL}

CFLAGS+=	-Wall -I${.CURDIR} -I${.CURDIR}/../switchd
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare

GENERATED=	ofp_map.c ofp10_map.c
CLEANFILES+=	${GENERATED}
YFLAGS=

ofp_map.c: genmap.sh ${.CURDIR}/ofp_map.h ${.CURDIR}/../../sys/net/ofp.h
	/bin/sh ${.CURDIR}/genmap.sh -i ${.CURDIR}/../../sys/net/ofp.h -t ofp \
	    -m ${.CURDIR}/ofp_map.h -h '<net/ofp.h>' > $@
	@touch $@

ofp10_map.c: genmap.sh ${.CURDIR}/ofp_map.h ${.CURDIR}/ofp10.h
	/bin/sh ${.CURDIR}/genmap.sh -i ${.CURDIR}/ofp10.h -t ofp10 \
	    -m ${.CURDIR}/ofp_map.h -h '"ofp10.h"' > $@
	@touch $@

.include <bsd.prog.mk>
