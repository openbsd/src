#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.6 1999/09/27 19:29:58 smurph Exp $

# 8Meg - 64K
RELOC=0x7F0000
      
S=	${.CURDIR}/../../../..
DEFS= 
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=-O0 ${INCPATH} ${DEFS} ${COPTS}
LDFLAGS=-N -T ${RELOC}
CLEANFILES+=bootsd

.include "${S}/arch/mvme88k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c version.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ} ${LIBGCC}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootsd 
ALL=	${BOOTS} 

all: ${ALL}

bootsd: ${OBJS} ${SDCRT} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
		${SDCRT} ${OBJS} ${LIBS}

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
