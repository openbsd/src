#	$OpenBSD: Makefile,v 1.46 2015/12/01 07:50:06 deraadt Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armish armv7 hppa i386 \
	landisk loongson luna88k macppc \
	octeon sgi socppc sparc sparc64 vax zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
