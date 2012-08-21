#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.13 2012/08/21 14:46:19 pascal Exp $

SIZE?=	size

S=	${.CURDIR}/../../../..
DEFS= 
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS+=-O2 ${INCPATH} ${DEFS} ${COPTS}
NOPIE=
CLEANFILES+=stboot bootst bootst.bug

.include "${S}/arch/mvme68k/stand/wrtvid/Makefile.inc"
.include "${S}/arch/mvme68k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c dev_tape.c rawfs.c version.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootst stboot
ALL=	${BOOTS} 

all: ${ALL}

bootst.bug:	${OBJS} ${BUGCRT} ${LIBS}
	${LD} -s -N -T ${STAGE2_RELOC} ${BUGCRT} ${OBJS} ${LIBS} \
	    `cc -print-libgcc-file-name` -o $@
	@${SIZE} bootst.bug

bootst stboot: bootst.bug ${WRTVID}
	${WRTVID} bootst.bug

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
