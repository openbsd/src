LIB=bug

CFLAGS+=-I${.CURDIR}/../include
CFLAGS+=-I${.CURDIR}/../../include
CFLAGS+=-I${.CURDIR}/../..
CFLAGS+=-I/usr/src/sys
CFLAGS+=-fwritable-strings

SRCS+=bugio.c
#SRCS+=bugcrt.c bugio.c main.c

.if (${MACHINE_ARCH} == "m68k")
SRCS+=mvme147.c bcopy.c memset.c
.endif
.if (${MACHINE_ARCH} == "m88k")
SRCS+=bcopy.c memset.c printf.c
.endif

.PATH:    ${.CURDIR}/../../../../lib/libc_sa ${.CURDIR}/${MACHINE_ARCH}

all:	bugio.o

#bugio.o: bugio.c
#	${CC} ${CFLAGS} -c -O ${.ALLSRC}
#	${LD} -x -r ${.TARGET}
#	mv a.out ${.TARGET}

.include <bsd.lib.mk>
