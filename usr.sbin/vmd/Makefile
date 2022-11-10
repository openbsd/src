#	$OpenBSD: Makefile,v 1.28 2022/11/10 11:46:39 dv Exp $

.if ${MACHINE} == "amd64"

PROG=		vmd
SRCS=		vmd.c control.c log.c priv.c proc.c config.c vmm.c
SRCS+=		vm.c loadfile_elf.c pci.c virtio.c i8259.c mc146818.c
SRCS+=		ns8250.c i8253.c dhcp.c packet.c mmio.c
SRCS+=		parse.y atomicio.c vioscsi.c vioraw.c vioqcow2.c fw_cfg.c
SRCS+=		vm_agentx.c

CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare

LDADD+=		-lutil -lpthread -levent -lz -lagentx
DPADD+=		${LIBUTIL} ${LIBPTHREAD} ${LIBEVENT} ${LIBZ} ${LIBAGENTX}

YFLAGS=

.else

NOPROG= yes

.endif

MAN=		vmd.8 vm.conf.5

.include <bsd.prog.mk>
