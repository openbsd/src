#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.15 2012/08/21 14:46:19 pascal Exp $

S=	${.CURDIR}/../../../..
DEFS=
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS+=${INCPATH} ${DEFS} ${COPTS}
LDFLAGS=-N -T ${STAGE2_RELOC}
NOPIE=
CLEANFILES+=bootsd

.include "${S}/arch/mvme88k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c version.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootsd
ALL=	${BOOTS}

all: ${ALL}

bootsd: ${OBJS} ${STAGE2_CRT} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
		${STAGE2_CRT} ${OBJS} ${LIBS} `cc -print-libgcc-file-name`

install:
	install ${INSTALL_STRIP} -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
