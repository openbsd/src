# $OpenBSD: Makefile,v 1.1 2016/07/19 16:54:26 reyk Exp $

NAME=		switch

PROG=		${NAME}d
MAN=		${NAME}d.8 ${NAME}d.conf.5

SRCS=		imsg_util.c log.c packet.c switch.c timer.c util.c
SRCS+=		switchd.c ofp.c ofp10.c ofp13.c control.c proc.c
SRCS+=		${.OBJDIR}/ofp_map.c ${.OBJDIR}/ofp10_map.c
SRCS+=		parse.y ofcconn.c

LDADD=		-levent -lutil
DPADD=		${LIBEVENT} ${LIBUTIL}

CFLAGS+=	-DSWITCHD_NAME=\"${NAME}\"
CFLAGS+=	-Wall -I${.CURDIR} -I${.CURDIR}/../${NAME}d
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare

GENERATED=	ofp_map.c ofp10_map.c
CLEANFILES+=	${GENERATED} *~

ofp_map.c: genmap.sh ${.CURDIR}/ofp.h
	/bin/sh ${.CURDIR}/genmap.sh -i ${.CURDIR}/ofp.h -t ofp \
	    -m ${.CURDIR}/ofp_map.h -h '"ofp.h"' > $@
	@touch $@

ofp10_map.c: genmap.sh ${.CURDIR}/ofp10.h
	/bin/sh ${.CURDIR}/genmap.sh -i ${.CURDIR}/ofp10.h -t ofp10 \
	    -m ${.CURDIR}/ofp_map.h -h '"ofp10.h"' > $@
	@touch $@

.include <bsd.prog.mk>
