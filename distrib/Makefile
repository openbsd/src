#	$OpenBSD: Makefile,v 1.56 2026/07/15 19:00:23 miod Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armv7 arm64 hppa i386 \
	landisk luna88k macppc \
	octeon powerpc64 riscv64 sparc64
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
