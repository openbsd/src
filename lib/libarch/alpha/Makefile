# $FreeBSD: src/lib/libio/Makefile,v 1.2 1999/08/28 00:04:42 peter Exp $
# $OpenBSD: Makefile,v 1.6 2002/07/12 22:22:43 deraadt Exp $

MAN=		libalpha.2
MLINKS+=	libalpha.2 dense_base.2 libalpha.2 inb.2 \
		libalpha.2 inl.2 libalpha.2 inw.2 \
		libalpha.2 ioperm.2 libalpha.2 map_memory.2 \
		libalpha.2 outb.2 libalpha.2 outl.2 \
		libalpha.2 outw.2 libalpha.2 readb.2 \
		libalpha.2 readl.2 libalpha.2 readw.2 \
		libalpha.2 unmap_memory.2 libalpha.2 writeb.2 \
		libalpha.2 writel.2 libalpha.2 writew.2
MANSUBDIR=alpha

.if ${MACHINE} == "alpha"
NOPIC=
SRCS=		io.c bwx.c
CFLAGS+= 	-Wall -Wno-uninitialized -Wa,-mev56
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif

