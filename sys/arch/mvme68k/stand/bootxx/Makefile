#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.1 1996/05/16 02:37:25 chuck Exp $

RELOC=0x3F0000 

S=	${.CURDIR}/../../../..
DEFS= 
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=	-O2 ${INCPATH} ${DEFS} ${COPTS}
CLEANFILES+=bootxx

.include "${S}/arch/${MACHINE}/stand/bugcrt/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/libbug/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/libsa/Makefile.inc"

SRCS=   bootxx.c conf.c version.c

LIBS=	${LIBSA} ${LIBBUG}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootxx
ALL=	${BOOTS} 

all: ${ALL}

bootxx: ${OBJS} ${BUGCRT} ${LIBS}
	${LD} -N -T ${RELOC} ${BUGCRT} ${OBJS} ${LIBS} -o $@

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
