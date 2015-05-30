# $FreeBSD: src/lib/libio/Makefile,v 1.2 1999/08/28 00:04:42 peter Exp $
# $OpenBSD: Makefile,v 1.8 2015/05/30 19:53:02 miod Exp $

MAN=		inb.2
MLINKS+=	inb.2 dense_base.2 \
		inb.2 inl.2 inb.2 inw.2 \
		inb.2 ioperm.2 inb.2 map_memory.2 \
		inb.2 outb.2 inb.2 outl.2 \
		inb.2 outw.2 inb.2 readb.2 \
		inb.2 readl.2 inb.2 readw.2 \
		inb.2 unmap_memory.2 inb.2 writeb.2 \
		inb.2 writel.2 inb.2 writew.2
MANSUBDIR=alpha

.if ${MACHINE} == "alpha"
NOPIC=
SRCS=		io.c bwx.c
CFLAGS+= 	-Wall -Wno-uninitialized -mbwx
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif

