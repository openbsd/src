#	$OpenBSD: Makefile,v 1.2 2016/08/31 22:11:15 florian Exp $
PROG=		acme-client
SRCS=		acctproc.c base64.c certproc.c chngproc.c dbg.c dnsproc.c
SRCS+=		fileproc.c http.c jsmn.c json.c keyproc.c main.c netproc.c
SRCS+=		revokeproc.c rsa.c sandbox-pledge.c util-pledge.c util.c
MAN=		acme-client.1

LDADD=		-ltls -lssl -lcrypto
DPADD=		${LIBTLS} ${LIBSSL} ${LIBCRYPTO}

CFLAGS		+= -g -W -Wall
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith
CFLAGS+=	-Wsign-compare

.include <bsd.prog.mk>
