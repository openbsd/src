#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.8 1997/04/22 16:13:40 gvf Exp $

RELOC=0x3F0000 
SIZE?=	size

S=	${.CURDIR}/../../../..
DEFS= 
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=	-O2 ${INCPATH} ${DEFS} ${COPTS}
CLEANFILES+=stboot bootst bootst.bug

.include "${S}/arch/${MACHINE}/stand/wrtvid/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/bugcrt/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/libbug/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/libsa/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c dev_tape.c rawfs.c version.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootst stboot
ALL=	${BOOTS} 

all: ${ALL}

bootst.bug:	${OBJS} ${BUGCRT} ${LIBS}
	${LD} -s -N -T ${RELOC} ${BUGCRT} ${OBJS} ${LIBS} -o $@
	@${SIZE} bootst.bug

bootst stboot: bootst.bug ${WRTVID}
	${WRTVID} bootst.bug

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
