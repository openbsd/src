
.if ${MACHINE} == "amd64"

PROG=	vmd
SRCS=	vmm.c loadfile_elf.c pci.c virtio.c
SRCS+=	vmd.c control.c log.c proc.c
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
LDADD+=	-lutil -lpthread -levent
DPADD+= ${LIBUTIL} ${LIBEVENT}

.else

NOPROG= yes

.endif

MAN= vmd.8

.include <bsd.prog.mk>
