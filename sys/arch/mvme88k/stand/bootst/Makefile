#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.4 1998/12/15 06:09:51 smurph Exp $

RELOC=0xAF0000 
SIZE?=	size

S=	${.CURDIR}/../../../..
DEFS= -DSTANDALONE -DCOMPAT_NOLABEL
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=	-O2 ${INCPATH} ${DEFS} ${COPTS}
CLEANFILES+=stboot bootst bootst.bug

.include "${S}/arch/mvme88k/stand/wrtvid/Makefile.inc"
.include "${S}/arch/mvme88k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme88k/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c dev_tape.c rawfs.c version.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ} libgcc.a

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootst stboot
ALL=	${BOOTS} 

all: ${ALL}

bootst.bug:	${OBJS} ${BUGCRT} ${LIBS}
	${LD} -s -N -T ${RELOC} ${BUGCRT} ${OBJS} ${LIBS} -o $@
	@${SIZE} bootst.bug

bootst stboot: bootst.bug ${WRTVID}
	${WRTVID} bootst.bug

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
