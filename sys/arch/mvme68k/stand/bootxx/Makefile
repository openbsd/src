#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.6 2001/08/05 20:37:37 miod Exp $

RELOC=0x10000 

S=	${.CURDIR}/../../../..
DEFS= 
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=	-O2 ${INCPATH} ${DEFS} ${COPTS}
CLEANFILES+=bootxx

.include "${S}/arch/mvme68k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libz/Makefile.inc"

SRCS=   bootxx.c conf.c version.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootxx
ALL=	${BOOTS} 

all: ${ALL}

bootxx: ${OBJS} ${BUGCRT} ${LIBS}
	${LD} -N -T ${RELOC} ${BUGCRT} ${OBJS} ${LIBS} -o $@

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
