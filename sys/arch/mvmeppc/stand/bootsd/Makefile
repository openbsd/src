#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.2 2001/08/21 17:44:16 miod Exp $

      
S=	${.CURDIR}/../../../..
DEFS= 
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=-O0 ${INCPATH} ${DEFS} ${COPTS}
LDFLAGS=-N -Ttext ${STAGE2_RELOC}
CLEANFILES+=bootsd

.include "${S}/arch/mvmeppc/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvmeppc/stand/libbug/Makefile.inc"
.include "${S}/arch/mvmeppc/stand/libsa/Makefile.inc"
.include "${S}/arch/mvmeppc/stand/libz/Makefile.inc"

SRCS=   boot.c version.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootsd 
ALL=	${BOOTS} 

all: ${ALL}

bootsd: ${OBJS} ${STAGE2} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
		${STAGE2} ${OBJS} ${LIBS}

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
