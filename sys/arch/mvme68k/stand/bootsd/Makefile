#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	     $Id: Makefile,v 1.1.1.1 1995/10/18 10:44:12 deraadt Exp $

RELOC=0x3F0000 

S=	${.CURDIR}/../../../..
DEFS= -DSTANDALONE -DCOMPAT_NOLABEL # -DROMPRF
INCPATH=-I${.CURDIR} -I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=	-O2 ${INCPATH} ${DEFS} ${COPTS}
CLEANFILES+=sdboot bootsd bootsd.bug

#.PATH: ${S}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}
#.PATH: ${S}/lib/libsa

.include "${S}/arch/${MACHINE}/stand/libsa/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/libbug/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/bugcrt/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/wrtvid/Makefile.inc"

SRCS=   boot.c filesystem.c bugdev.c version.c

LIBS=	${LIBSA} ${LIBBUG}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootsd sdboot
ALL=	${BOOTS} 

all: ${ALL}

devopen.o machdep.o: Makefile

bootsd.bug:	${OBJS} ${BUGCRT} ${LIBS}
	${LD} -s -N -T ${RELOC} ${BUGCRT} ${OBJS} ${LIBS} -o $@
	@size bootsd.bug

bootsd sdboot: bootsd.bug ${WRTVID}
	${WRTVID} bootsd.bug

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
