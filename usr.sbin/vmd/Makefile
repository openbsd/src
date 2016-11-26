#	$OpenBSD: Makefile,v 1.12 2016/11/26 20:03:42 reyk Exp $

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"

PROG=		vmd
SRCS=		vmm.c loadfile_elf.c pci.c virtio.c i8259.c mc146818.c
SRCS+=		vmd.c control.c log.c priv.c proc.c config.c ns8250.c i8253.c
SRCS+=		vmboot.c ufs.c disklabel.c
SRCS+=		parse.y

CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare

LDADD+=		-lutil -lpthread -levent
DPADD+=		${LIBUTIL} ${LIBEVENT}

CLEANFILES+=	y.tab.h

.else

NOPROG= yes

.endif

MAN=		vmd.8 vm.conf.5

.include <bsd.prog.mk>
