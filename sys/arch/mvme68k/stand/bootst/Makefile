#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	     $Id: Makefile,v 1.2 1995/10/23 03:38:29 deraadt Exp $

RELOC=0x3F0000 

S=	${.CURDIR}/../../../..
DEFS= -DSTANDALONE -DCOMPAT_NOLABEL # -DROMPRF
INCPATH=-I${.CURDIR} -I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=	-O2 ${INCPATH} ${DEFS} ${COPTS}
CLEANFILES+=sdboot bootst bootst.bug

#.PATH: ${S}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}
#.PATH: ${S}/lib/libsa

.include "${S}/arch/${MACHINE}/stand/libsa/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/libbug/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/bugcrt/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/wrtvid/Makefile.inc"

SRCS=   bootst.c

LIBS=	${LIBSA} ${LIBBUG}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootst stboot
ALL=	${BOOTS} 

all: ${ALL}

bootst.bug:	${OBJS} ${BUGCRT} ${LIBS}
	${LD} -s -N -T ${RELOC} ${BUGCRT} ${OBJS} ${LIBS} -o $@
	@size bootst.bug

bootst stboot: bootst.bug ${WRTVID}
	${WRTVID} bootst.bug

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
