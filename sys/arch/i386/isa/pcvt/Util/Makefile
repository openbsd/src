#	$OpenBSD: Makefile,v 1.5 1999/11/25 21:18:01 deraadt Exp $

SUBDIR= 	keycap cursor fontedit fonts kcon loadfont scon \
		userkeys vttest ispcvt pcvtdoc fed

.if make(clean) || make(cleandir) || make(obj)
SUBDIR+=	vgaio kbdio set2061
SUBDIR+=	demo
.endif

includes:
	-cd ${.CURDIR}; cmp -s ../pcvt_ioctl.h \
	    ${DESTDIR}/usr/include/machine/pcvt_ioctl.h > /dev/null 2>&1 || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 ../pcvt_ioctl.h \
	    ${DESTDIR}/usr/include/machine

.include <bsd.subdir.mk>
