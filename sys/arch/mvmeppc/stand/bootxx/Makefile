#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.2 2001/08/21 17:44:19 miod Exp $

S=	${.CURDIR}/../../../..
DEFS=-DSTAGE2_RELOC=${STAGE2_RELOC} 
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=-O0 ${INCPATH} ${DEFS} ${COPTS}
CLEANFILES+=bootxx

.include "${S}/arch/mvmeppc/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvmeppc/stand/libbug/Makefile.inc"
.include "${S}/arch/mvmeppc/stand/libsa/Makefile.inc"
.include "${S}/arch/mvmeppc/stand/libz/Makefile.inc"

SRCS=   bootxx.c version.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootxx
ALL=	${BOOTS} 

all: ${ALL}

bootxx: ${OBJS} ${STAGE1} ${LIBS}
	${LD} -X -N -Ttext ${STAGE1_RELOC} ${STAGE1} ${OBJS} ${LIBS} -o $@

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
