STRIP_AOUT_HDR=rmaouthdr
all:	bootst.bug
STRIP=-s
SRCS=bootst.c
S=${.CURDIR}/../../../..

CFLAGS+=-I${.CURDIR}/../../include -I${.CURDIR}/${MACHINE_ARCH}

.include "${S}/arch/${MACHINE}/stand/libbug/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/bugcrt/Makefile.inc"
.include "${S}/lib/libsa/Makefile.inc"
.include "${S}/lib/libc_sa/Makefile.inc"
.include "${S}/lib/libkern/Makefile.inc"

LDADD+=${LIBBUG} ${SA_LIB} ${C_SA_LIB} ${KERNLIB}

bootst.bug:	bootst.o ${LDADD} ${BUGCRT}
	${LD} ${STRIP} -N -T 0x3F0000 ${BUGCRT} bootst.o ${LDADD} -o bootst.bug
	${.CURDIR}/../wrtvid/wrtvid bootst.bug

clean:
	rm -f a.out *.core
	rm -f bootst.o bootst.bug stboot bootst

install:
	install -c -m 555 -g bin -o bin bootst stboot /usr/mdec/

.include <bsd.prog.mk>
