OBJ=bugcrt.o
CFLAGS+=-I${.CURDIR}/../include
CFLAGS+=-I${.CURDIR}/../..
CFLAGS+=-I/usr/src/sys
CFLAGS+=-fwritable-strings

SRCS=bugcrt.c
all:	bugcrt.o

.include <bsd.prog.mk>
