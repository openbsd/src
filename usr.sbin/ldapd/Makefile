#	$OpenBSD: Makefile,v 1.10 2013/08/06 19:05:57 miod Exp $

PROG=		ldapd
MAN=		ldapd.8 ldapd.conf.5
SRCS=		ber.c log.c control.c \
		util.c ldapd.c ldape.c conn.c attributes.c namespace.c \
		btree.c filter.c search.c parse.y \
		auth.c modify.c index.c ssl.c ssl_privsep.c \
		validate.c uuid.c schema.c imsgev.c syntax.c matching.c

LDADD=		-levent -lssl -lcrypto -lz -lutil
DPADD=		${LIBEVENT} ${LIBCRYPTO} ${LIBSSL} ${LIBZ} ${LIBUTIL}
CFLAGS+=	-I${.CURDIR} -g
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CLEANFILES+=	y.tab.h parse.c

.if ${MACHINE} == "vax"
LDFLAGS+=-Wl,--no-keep-memory
.endif

SCHEMA_FILES=	core.schema \
		inetorgperson.schema \
		nis.schema

distribution:
	for i in ${SCHEMA_FILES}; do \
		${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/schema/$$i ${DESTDIR}/etc/ldap/; \
	done

.include <bsd.prog.mk>

