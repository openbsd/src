
.if ${MACHINE} == "amd64"

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
