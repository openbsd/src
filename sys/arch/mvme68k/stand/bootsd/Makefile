#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.6 1996/05/16 02:46:56 chuck Exp $

RELOC=0x3F0000 

S=	${.CURDIR}/../../../..
DEFS= 
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=	-O2 ${INCPATH} ${DEFS} ${COPTS}
CLEANFILES+=bootsd

.include "${S}/arch/${MACHINE}/stand/bugcrt/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/libbug/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/libsa/Makefile.inc"

SRCS=   boot.c conf.c version.c

LIBS=	${LIBSA} ${LIBBUG}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootsd 
ALL=	${BOOTS} 

all: ${ALL}

bootsd: ${OBJS} ${LIBSA} ${BUGCRT} ${LIBBUG}
	${LD} -N -T ${RELOC} -o $@ \
		${SRTOBJ} ${BUGCRT} ${OBJS} ${LIBSA} ${LIBBUG} 

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
