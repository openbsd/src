#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.11 2013/02/02 13:36:06 miod Exp $

S=	${.CURDIR}/../../../..
DEFS=-DSTAGE2_RELOC=${STAGE2_RELOC}
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS+=-O2 ${INCPATH} ${DEFS} ${COPTS}
LDFLAGS=-S -N -T ${S}/arch/mvme68k/stand/ld.script -Ttext ${STAGE1_RELOC} \
	-e __bootxx_start
NOPIE=
CLEANFILES+=bootxx

.include "${S}/arch/mvme68k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libz/Makefile.inc"

SRCS=   bootxx.c conf.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootxx
ALL=	${BOOTS} 

all: ${ALL}

bootxx: ${OBJS} ${BUGCRT} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
	    ${BUGCRT} ${OBJS} ${LIBS} `${CC} -print-libgcc-file-name`

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
