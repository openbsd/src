#	$OpenBSD: Makefile,v 1.7 1999/11/18 09:03:39 deraadt Exp $

LIB	=	keycap
CAPDIR	=	/usr/share/misc
CAPPATH	=	$(CAPDIR)/keycap.pcvt
KEYCAPSRC=	keycap.src
CFLAGS	+=	-DKEYCAP_PATH=\"$(CAPPATH)\"
SRCS	=	keycap.c
MAN3	=	keycap.3 keycap.5
MLINKS	=	keycap.3 kgetent.3 keycap.3 kgetnum.3 keycap.3 kgetflag.3 \
		keycap.3 kgetstr.3

beforeinstall:
	-@if [ ! -d ${DESTDIR}${LIBDIR} ]; then \
		mkdir ${DESTDIR}${LIBDIR}; \
	fi
	install -c -m ${LIBMODE} -o ${LIBOWN} -g ${LIBGRP} \
	    ${.CURDIR}/${KEYCAPSRC} ${DESTDIR}${CAPPATH}

.include <bsd.lib.mk>
