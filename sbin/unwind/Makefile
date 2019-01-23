#	$OpenBSD: Makefile,v 1.1 2019/01/23 13:11:00 florian Exp $

PROG=	unwind
SRCS=	control.c resolver.c frontend.c uw_log.c unwind.c uw_parse.y printconf.c
SRCS+=	asr_utils.c
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
LDADD+=	-levent -lutil -lssl -lcrypto
DPADD+= ${LIBEVENT} ${LIBUTIL} ${LIBSSL} ${LIBCRYPTO}

.include <bsd.prog.mk>
