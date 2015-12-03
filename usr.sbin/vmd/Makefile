#	$OpenBSD: Makefile,v 1.7 2015/12/03 16:11:32 reyk Exp $

.if ${MACHINE} == "amd64"

PROG=		vmd
SRCS=		vmm.c loadfile_elf.c pci.c virtio.c
SRCS+=		vmd.c control.c log.c proc.c config.c
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
