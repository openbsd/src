PROG	= pts
SRCS	= pts.c
LDADD	+= -lkafs
DPADD	+= ${LIBKAFS}

.include "../Applflags.inc"

.PATH: ${.CURDIR}/../src/appl/pts

SRCS	+= arlalib.c fs_lib.c
CFLAGS	+= -I${.CURDIR}/../src/appl/lib
.PATH: ${.CURDIR}/../src/appl/lib
.PATH: ${.CURDIR}/../src/arlad

.include <bsd.prog.mk>
