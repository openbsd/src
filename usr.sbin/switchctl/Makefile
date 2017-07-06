# $OpenBSD: Makefile,v 1.5 2017/07/06 12:15:23 espie Exp $

SWITCHD=	${.CURDIR}/../switchd

.PATH:		${SWITCHD}

PROG=		switchctl
MAN=		switchctl.8
SRCS=		log.c ofpclient.c parser.c util.c imsg_util.c switchctl.c
SRCS+=		ofp10.c ofp13.c ofp_common.c packet.c

LDADD=		-lutil
DPADD=		${LIBUTIL}

CFLAGS+=	-Wall -I${.CURDIR} -I${SWITCHD}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare

GENERATED=	ofp_map.c ofp10_map.c
SRCS+=		${GENERATED}
CLEANFILES+=	${GENERATED}

ofp_map.c: genmap.sh ${SWITCHD}/ofp_map.h ${SWITCHD}/../../sys/net/ofp.h
	/bin/sh ${SWITCHD}/genmap.sh -i ${SWITCHD}/../../sys/net/ofp.h -t ofp \
	    -m ${SWITCHD}/ofp_map.h -h '<net/ofp.h>' > $@
	@touch $@

ofp10_map.c: genmap.sh ${SWITCHD}/ofp_map.h ${SWITCHD}/ofp10.h
	/bin/sh ${SWITCHD}/genmap.sh -i ${SWITCHD}/ofp10.h -t ofp10 \
	    -m ${SWITCHD}/ofp_map.h -h '"ofp10.h"' > $@
	@touch $@

.include <bsd.prog.mk>
