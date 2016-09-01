#	$OpenBSD: Makefile,v 1.8 2016/09/01 15:38:03 mlarkin Exp $

.if ${MACHINE} == "amd64"

PROG=		vmd
SRCS=		vmm.c loadfile_elf.c pci.c virtio.c i8259.c mc146818.c
SRCS+=		vmd.c control.c log.c proc.c config.c ns8250.c i8253.c
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
