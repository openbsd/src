
PROG=		ctfdump
SRCS=		ctfdump.c

CFLAGS+=	-W -Wall -Wstrict-prototypes -Wno-unused -Wunused-variable

CFLAGS+=	-DZLIB
LDADD+=		-lelf -lz
DPADD+=		${LIBELF} ${LIBZ}

.include <bsd.prog.mk>
