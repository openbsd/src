PROG	= pts
SRCS	= pts.c arlalib.c fs_lib.c
LDADD	+= -lkafs
DPADD	+= ${LIBKAFS}

.include "../Applflags.inc"

.PATH: ${.CURDIR}/../src/appl

.include <bsd.prog.mk>
