#	$OpenBSD: Makefile,v 1.48 2016/08/10 14:27:15 deraadt Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armv7 hppa i386 \
	landisk loongson luna88k macppc \
	octeon sgi socppc sparc sparc64 zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
