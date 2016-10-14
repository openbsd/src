#	$OpenBSD: Makefile,v 1.1 2016/10/14 18:42:59 deraadt Exp $

.if	make(obj) || make(clean)
SUBDIR!=find . -type d -maxdepth 1 \! \( -name . -o -name CVS \)
#SUBDIR=GENERIC GENERIC.MP RAMDISK RAMDISKB RAMDISKBIG RAMDISKC
.endif

.include <bsd.subdir.mk>
