
.if ${MACHINE} == "amd64"

PROG=	vmd
SRCS=	vmd.c loadfile_elf.c pci.c virtio.c
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-lutil -lpthread
DPADD+= ${LIBUTIL}

.else

NOPROG= yes

.endif

MAN= vmd.8
MANSUBDIR=${MACHINE}

.include <bsd.prog.mk>
