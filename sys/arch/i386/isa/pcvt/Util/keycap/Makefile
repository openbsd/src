#	$OpenBSD: Makefile,v 1.10 2000/05/26 11:50:45 deraadt Exp $

MAN3	=	keycap.3
MAN5	=	keycap.5
MLINKS	=	keycap.3 kgetent.3 keycap.3 kgetnum.3 keycap.3 kgetflag.3 \
		keycap.3 kgetstr.3
MANSUBDIR=/i386

.if ${MACHINE} == "i386"
LIB	=	keycap
CAPDIR	=	/usr/share/misc
CAPPATH	=	$(CAPDIR)/keycap.pcvt
KEYCAPSRC=	keycap.src
CFLAGS	+=	-DKEYCAP_PATH=\"$(CAPPATH)\"
SRCS	=	keycap.c

beforeinstall:
	-@if [ ! -d ${DESTDIR}${LIBDIR} ]; then \
		mkdir ${DESTDIR}${LIBDIR}; \
	fi
	install -c -m ${LIBMODE} -o ${LIBOWN} -g ${LIBGRP} \
	    ${.CURDIR}/${KEYCAPSRC} ${DESTDIR}${CAPPATH}
.include <bsd.lib.mk>
.else
NOPROG=
.include <bsd.prog.mk>
.endif

