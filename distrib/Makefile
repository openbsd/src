#	$OpenBSD: Makefile,v 1.49 2016/09/01 10:08:28 deraadt Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armv7 hppa i386 \
	landisk loongson luna88k macppc \
	octeon sgi socppc sparc64 zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
