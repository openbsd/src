#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.10 1998/08/18 22:58:15 deraadt Exp $

RELOC=0x3F0000 

S=	${.CURDIR}/../../../..
DEFS= 
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=	-O2 ${INCPATH} ${DEFS} ${COPTS}
LDFLAGS=-N -T ${RELOC}
CLEANFILES+=bootsd

.include "${S}/arch/mvme68k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c version.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ} libgcc.a

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootsd 
ALL=	${BOOTS} 

all: ${ALL}

bootsd: ${OBJS} ${BUGCRT} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
		${BUGCRT} ${OBJS} ${LIBS}
#${SRTOBJ}
install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
