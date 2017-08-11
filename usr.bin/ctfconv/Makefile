
PROG=		ctfconv
SRCS=		ctfconv.c parse.c elf.c dw.c generate.c hash.c xmalloc.c \
		pool.c

CFLAGS+=	-W -Wall -Wstrict-prototypes -Wno-unused -Wunused-variable \
		-Wno-unused-parameter

CFLAGS+=	-DZLIB
LDADD+=		-lz
DPADD+=		${LIBZ}

MAN=		ctfconv.1 ctfstrip.1

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/ctfstrip ${DESTDIR}${BINDIR}/ctfstrip

.include <bsd.prog.mk>
