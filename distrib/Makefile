#	$OpenBSD: Makefile,v 1.54 2021/04/28 15:11:27 jsg Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armv7 arm64 hppa i386 \
	landisk loongson luna88k macppc \
	octeon powerpc64 riscv64 sgi sparc64
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
