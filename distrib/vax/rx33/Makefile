#	$OpenBSD: Makefile,v 1.2 2000/03/01 22:10:14 todd Exp $
#	$NetBSD: Makefile,v 1.1 1995/10/01 21:22:40 ragge Exp $
#
#	Create RX33 floppies.

.include "../Makefile.inc"

all:
	cp ../inst/bootroot-${REV}.fs rx33-bootdisk-${REV}
	dd if=../inst/miniroot-${REV}.fs of=rx33-copy1-${REV} bs=512 count=2400
	dd if=../inst/miniroot-${REV}.fs of=rx33-copy2-${REV} \
		bs=512 skip=2400 count=2400

.include <bsd.obj.mk>  
.include <bsd.subdir.mk>
