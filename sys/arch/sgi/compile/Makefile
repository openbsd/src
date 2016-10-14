#	$OpenBSD: Makefile,v 1.1 2016/10/14 18:43:00 deraadt Exp $

.if	make(obj) || make(clean)
SUBDIR!=find . -type d -maxdepth 1 \! \( -name . -o -name CVS \)
#SUBDIR=GENERIC-IP22 GENERIC-IP26 GENERIC-IP27 GENERIC-IP27.MP \
#	GENERIC-IP28 GENERIC-IP30 GENERIC-IP30.MP GENERIC-IP32 \
#	RAMDISK-IP22 RAMDISK-IP26 RAMDISK-IP27 RAMDISK-IP28 \
#	RAMDISK-IP30 RAMDISK-IP32
.endif

.include <bsd.subdir.mk>
