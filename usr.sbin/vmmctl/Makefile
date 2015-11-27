
.if ${MACHINE} == "amd64"

PROG=	vmmctl
SRCS=	vmmctl.c main.c parse.y
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

MAN= vmmctl.8 vmm.conf.5

.include <bsd.prog.mk>
