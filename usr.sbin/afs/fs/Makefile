PROG	= fs
BINDIR	= /usr/sbin
SRCS	= fs.c arlalib.c fs_lib.c
LDADD	+= -lkafs
DPADD	+= ${LIBKAFS}

.include "../Applflags.inc"

.PATH: ${.CURDIR}/../src/appl

SRCS	+= arladeb.c

.PATH: ${.CURDIR}/../src/arlad

.include <bsd.prog.mk>
