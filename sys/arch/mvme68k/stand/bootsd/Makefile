#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.19 2013/02/02 13:36:06 miod Exp $

STRIP?=	strip

S=	${.CURDIR}/../../../..
DEFS= 
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS+=-O2 ${INCPATH} ${DEFS} ${COPTS}
LDFLAGS=-N -T ${S}/arch/mvme68k/stand/ld.script -Ttext ${STAGE2_RELOC}
NOPIE=
CLEANFILES+=bootsd bootsd.strip

.include "${S}/arch/mvme68k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c version.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ} 

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootsd 
ALL=	${BOOTS} 

all: ${ALL}

bootsd: ${BUGCRT} ${OBJS} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
	    ${BUGCRT} ${OBJS} ${LIBS} `${CC} -print-libgcc-file-name`

install: bootsd
	${STRIP} -I a.out-m68k-netbsd -o bootsd.strip bootsd
	install -c -m 555 -g bin -o bin bootsd.strip ${DESTDIR}${MDEC_DIR}/bootsd

.include <bsd.prog.mk>
