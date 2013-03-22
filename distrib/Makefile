#	$OpenBSD: Makefile,v 1.41 2013/03/22 22:35:01 aoyama Exp $

SUBDIR=	special notes

.if	make(obj)
SUBDIR+=alpha amd64 armish beagle hp300 hppa i386 landisk loongson luna88k \
	macppc mvme68k mvme88k sgi socppc sparc sparc64 vax zaurus
.elif exists(${MACHINE})
SUBDIR+=${MACHINE}
.endif

.include <bsd.subdir.mk>
