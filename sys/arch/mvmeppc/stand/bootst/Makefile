#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.2 2001/08/25 20:36:54 miod Exp $

SIZE?=	size

S=	${.CURDIR}/../../../..
DEFS= -DSTANDALONE -DCOMPAT_NOLABEL
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=-O0 ${INCPATH} ${DEFS} ${COPTS}
LDFLAGS=-s -N -Ttext ${STAGE2_RELOC}
CLEANFILES+=stboot bootst bootst.bug

#.include "${S}/arch/mvmeppc/stand/wrtvid/Makefile.inc"
.include "${S}/arch/mvmeppc/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvmeppc/stand/libbug/Makefile.inc"
.include "${S}/arch/mvmeppc/stand/libsa/Makefile.inc"
.include "${S}/arch/mvmeppc/stand/libz/Makefile.inc"

SRCS=   boot.c version.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ} 

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootst
ALL=	${BOOTS} 

all: ${ALL}

bootst:	${OBJS} ${SINGLE} ${LIBS}
	${LD} ${LDFLAGS} ${SINGLE} ${OBJS} ${LIBS} -o $@
	@${SIZE} $@

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
