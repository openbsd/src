#	$OpenBSD: Makefile.inc,v 1.1 2026/06/04 05:43:00 deraadt Exp $

LDADD+=		-levent -lutil -ltls -lssl -lcrypto -lz
DPADD+=		${LIBEVENT} ${LIBUTIL} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO} ${LIBZ}

CFLAGS+=	-fstack-protector-all
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-Werror-implicit-function-declaration
#CFLAGS+=	-Werror # during development phase (breaks some archs)
CFLAGS+=	-DIO_TLS
CFLAGS+=	-DQUEUE_PROFILING
YFLAGS=
