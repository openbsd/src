#	$OpenBSD: Makefile,v 1.2 2000/03/01 22:10:15 todd Exp $
#	$NetBSD: Makefile,v 1.1 1995/10/01 21:22:42 ragge Exp $
#
#	Create RX50 floppies.

.include "../Makefile.inc"

all:
	cp ../inst/bootroot-${REV}.fs rx50-bootdisk-${REV}
	dd if=../inst/miniroot-${REV}.fs of=rx50-copy1-${REV} bs=512 count=800
	dd if=../inst/miniroot-${REV}.fs of=rx50-copy2-${REV} \
		bs=512 skip=800 count=800
	dd if=../inst/miniroot-${REV}.fs of=rx50-copy3-${REV} \
		bs=512 skip=1600 count=800
	dd if=../inst/miniroot-${REV}.fs of=rx50-copy4-${REV} \
		bs=512 skip=2400 count=800
	dd if=../inst/miniroot-${REV}.fs of=rx50-copy5-${REV} \
		bs=512 skip=3200 count=800
	dd if=../inst/miniroot-${REV}.fs of=rx50-copy6-${REV} \
		bs=512 skip=4000 count=800

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
