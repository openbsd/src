PROG	= fs
BINDIR	= /usr/sbin
LDADD	+= -lkafs
DPADD	+= ${LIBKAFS}

.include "../Applflags.inc"

SRCS	= fs.c
.PATH: ${.CURDIR}/../src/appl/fs

SRCS	+= arladeb.c arlalib.c fs_lib.c
CFLAGS	+= -I${.CURDIR}/../src/appl/lib
.PATH: ${.CURDIR}/../src/appl/lib
.PATH: ${.CURDIR}/../src/arlad

.include <bsd.prog.mk>
