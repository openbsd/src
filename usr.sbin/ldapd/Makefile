#	$OpenBSD: Makefile,v 1.5 2010/06/23 12:40:19 martinh Exp $

PROG=		ldapd
MAN=		ldapd.8 ldapd.conf.5
SRCS=		ber.c log.c control.c \
		util.c ldapd.c ldape.c conn.c attributes.c namespace.c \
		btree.c filter.c search.c parse.y \
		auth.c modify.c index.c ssl.c ssl_privsep.c \
		validate.c uuid.c

LDADD=		-levent -lssl -lcrypto -lz -lutil
DPADD=		${LIBEVENT} ${LIBCRYPTO} ${LIBSSL} ${LIBZ} ${LIBUTIL}
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CLEANFILES+=	y.tab.h parse.c

SCHEMA_FILES=	core.schema \
		inetorgperson.schema \
		nis.schema

distribution:
	for i in ${SCHEMA_FILES}; do \
		${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/schema/$$i ${DESTDIR}/etc/ldap/; \
	done

.include <bsd.prog.mk>

