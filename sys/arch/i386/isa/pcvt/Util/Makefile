#	$OpenBSD: Makefile,v 1.7 2000/09/01 05:46:02 aaron Exp $

SUBDIR=	keycap cursor fontedit kcon loadfont scon \
	userkeys vttest ispcvt pcvtdoc fed
.if ${MACHINE} == "i386"
SUBDIR+=fonts
.endif

.if make(clean) || make(cleandir) || make(obj)
SUBDIR+=	vgaio kbdio set2061
SUBDIR+=	demo
.endif

includes:
	-cd ${.CURDIR}; cmp -s ../pcvt_ioctl.h \
	    ${DESTDIR}/usr/include/machine/pcvt_ioctl.h > /dev/null 2>&1 || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 ../pcvt_ioctl.h \
	    ${DESTDIR}/usr/include/machine
	-cd ${.CURDIR}; cmp -s ../pcvt_mouse.h \
	    ${DESTDIR}/usr/include/machine/pcvt_mouse.h > /dev/null 2>&1 || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 ../pcvt_mouse.h \
	    ${DESTDIR}/usr/include/machine

.include <bsd.subdir.mk>
