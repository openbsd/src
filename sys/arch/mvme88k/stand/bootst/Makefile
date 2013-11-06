#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.18 2013/11/06 19:37:06 miod Exp $

S=	${.CURDIR}/../../../..
DEFS= -DSTANDALONE -DCOMPAT_NOLABEL
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS+=${INCPATH} ${DEFS} ${COPTS}
LDFLAGS=-N -T ${S}/arch/mvme88k/stand/ld.script -Ttext ${STAGE2_RELOC}
LDFLAGS+= -warn-common
NOPIE=
CLEANFILES+=stboot bootst bootst.bug

.include "${S}/arch/mvme88k/stand/wrtvid/Makefile.inc"
.include "${S}/arch/mvme88k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c dev_tape.c rawfs.c version.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootst stboot
ALL=	${BOOTS}

all: ${ALL}

bootst.bug: ${STAGE1_CRT} ${OBJS} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
	    ${STAGE1_CRT} ${OBJS} ${LIBS} `${CC} -print-libgcc-file-name`

bootst stboot: bootst.bug ${WRTVID}
	${WRTVID} bootst.bug

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
