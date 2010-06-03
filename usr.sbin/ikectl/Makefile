# $OpenBSD: Makefile,v 1.1 2010/06/03 16:41:12 reyk Exp $
# $vantronix: Makefile,v 1.9 2010/06/03 15:57:59 reyk Exp $

.PATH:		${.CURDIR}/../../sbin/iked

PROG=		ikectl
SRCS=		log.c ikeca.c ikectl.c parser.c

MAN=		ikectl.8

LDADD=		-lutil -lcrypto
DPADD=		${LIBUTIL}
CFLAGS+=	-Wall -I${.CURDIR} -I${.CURDIR}/../../sbin/iked
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded

#distribution:
#	${INSTALL} -C -g wheel -m 0644 ${.CURDIR}/ikeca.cnf \
#	    ${DESTDIR}/etc/ssl/ikeca.cnf

.include <bsd.prog.mk>
