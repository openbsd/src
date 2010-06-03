# $OpenBSD: Makefile,v 1.1 2010/06/03 16:41:12 reyk Exp $
# $vantronix: Makefile,v 1.22 2010/06/02 12:22:58 reyk Exp $

PROG=		iked
SRCS=		dh.c iked.c ikev1.c ikev2.c log.c \
		proc.c util.c config.c policy.c crypto.c ca.c pfkey.c \
		control.c chap_ms.c eap.c
SRCS+=		parse.y
SRCS+=		${.OBJDIR}/ikev2_map.c ${.OBJDIR}/eap_map.c
MAN=		iked.conf.5 iked.8
#NOMAN=		yes

BINDIR=		/sbin

LDADD=		-lutil -levent -lssl -lcrypto
DPADD=		${LIBUTIL} ${LIBEVENT} ${LIBSSL} ${LIBCRYPTO}
CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CLEANFILES+=	y.tab.h ikev2_map.c eap_map.c
GENERATED=	ikev2_map.c eap_map.c

ikev2_map.c: genmap.sh ikev2.h
	/bin/sh ${.CURDIR}/genmap.sh ${.CURDIR}/ikev2.h ikev2 > $@
	@touch $@

eap_map.c: genmap.sh eap.h
	/bin/sh ${.CURDIR}/genmap.sh ${.CURDIR}/eap.h eap > $@
	@touch $@

${PROG} beforedepend: ${GENERATED}

.include <bsd.prog.mk>
