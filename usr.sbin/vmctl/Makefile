#	$OpenBSD: Makefile,v 1.3 2016/10/26 05:26:36 mlarkin Exp $

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"

PROG=	vmctl
SRCS=	vmctl.c main.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../vmd
LDADD+=	-lutil
DPADD+= ${LIBUTIL}

.else

NOPROG= yes

.endif

MAN= vmctl.8

.include <bsd.prog.mk>
