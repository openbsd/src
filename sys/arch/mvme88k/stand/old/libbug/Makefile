LIB=bug

CFLAGS+=-I${.CURDIR}/../include
CFLAGS+=-I${.CURDIR}/../../include
CFLAGS+=-I${.CURDIR}/../..
CFLAGS+=-I/usr/src/sys
CFLAGS+=-fwritable-strings

NOPIC=

SRCS+=bug.c
#SRCS+=bugcrt.c bugio.c main.c

.if (${MACHINE_ARCH} == "m68k")
SRCS+=mvme147.c bcopy.c bzero.c
.endif
.if (${MACHINE_ARCH} == "m88k")
SRCS+=bcopy.c bzero.c printf.c bugsupp.c
.endif

.PATH:    ${.CURDIR}/../../../../lib/libkern ${.CURDIR}/../../../../lib/libsa ${.CURDIR}/${MACHINE_ARCH}

all:	bugio.o

#bugio.o: bugio.c
#	${CC} ${CFLAGS} -c -O ${.ALLSRC}
#	${LD} -x -r ${.TARGET}
#	mv a.out ${.TARGET}

.include <bsd.lib.mk>
