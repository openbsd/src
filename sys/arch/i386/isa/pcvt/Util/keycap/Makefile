#	$OpenBSD: Makefile,v 1.6 1999/11/17 04:55:36 millert Exp $

LIB	=	keycap
CAPDIR	=	/usr/share/misc
CAPPATH	=	$(CAPDIR)/keycap.pcvt
KEYCAPSRC=	keycap.src
CFLAGS	+=	-DKEYCAP_PATH=\"$(CAPPATH)\"
SRCS	=	keycap.c
MAN	=	keycap.3 keycap.5
MLINKS	=	keycap.3 kgetent.3 keycap.3 kgetnum.3 keycap.3 kgetflag.3 \
		keycap.3 kgetstr.3

beforeinstall:
	-@if [ ! -d ${DESTDIR}${LIBDIR} ]; then \
		mkdir ${DESTDIR}${LIBDIR}; \
	fi
	install -c -m ${LIBMODE} -o ${LIBOWN} -g ${LIBGRP} \
	    ${.CURDIR}/${KEYCAPSRC} ${DESTDIR}${CAPPATH}

.include <bsd.lib.mk>
