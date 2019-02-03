#	$OpenBSD: Makefile,v 1.3 2019/02/03 12:02:30 florian Exp $

PROG=	unwind
SRCS=	control.c resolver.c frontend.c uw_log.c unwind.c uw_parse.y printconf.c
SRCS+=	captiveportal.c
MAN=	unwind.8 unwind.conf.5

.include "${.CURDIR}/libunbound/Makefile.inc"

#DEBUG=	-g -DDEBUG=3 -O0
CFLAGS+= -Wall -I${.CURDIR} -I ${.CURDIR}/libunbound/libunbound
CFLAGS+= -I ${.CURDIR}/libunbound
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-levent -lutil -ltls -lssl -lcrypto
DPADD+= ${LIBEVENT} ${LIBUTIL} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO}

.include <bsd.prog.mk>
