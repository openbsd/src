LIB	=	keycap
CAPDIR	=	$(DESTDIR)/usr/share/misc
CAPPATH	=	$(CAPDIR)/keycap.pcvt
KEYCAPSRC=	keycap.src
CFLAGS	+=	-DKEYCAP_PATH=\"$(CAPPATH)\"
SRCS	=	keycap.c
MAN3	=	keycap.${MAN3EXT}
MAN5	=	keycap.${MAN5EXT}
#CLEANFILES+=	keycap.0 keycap.0

beforeinstall:
	@if [ ! -d ${DESTDIR}${LIBDIR} ]; then mkdir ${DESTDIR}${LIBDIR};fi
	@${ECHO} "Saving old keycap database"
	-mv -f ${DESTDIR}${CAPPATH} ${DESTDIR}${CAPPATH}.BAK
	install -c -m ${LIBMODE} -o ${LIBOWN} -g ${LIBGRP} \
	${.CURDIR}/${KEYCAPSRC} ${DESTDIR}${CAPPATH}


.include <bsd.lib.mk>
