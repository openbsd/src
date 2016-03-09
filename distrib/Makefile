#	$OpenBSD: Makefile,v 1.47 2016/03/09 16:28:45 deraadt Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armish armv7 hppa i386 \
	landisk loongson luna88k macppc \
	octeon sgi socppc sparc sparc64 zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
