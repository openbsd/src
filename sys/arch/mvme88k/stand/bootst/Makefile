#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.6 1999/09/27 19:29:59 smurph Exp $

# 8Meg - 64K
RELOC=0x7F0000 
SIZE?=	size

S=	${.CURDIR}/../../../..
DEFS= -DSTANDALONE -DCOMPAT_NOLABEL
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=-O0 ${INCPATH} ${DEFS} ${COPTS}
LDFLAGS=-s -N -T ${RELOC}
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

bootst.bug:	${OBJS} ${BUGCRT} ${LIBS}
	${LD} ${LDFLAGS} ${BUGCRT} ${OBJS} ${LIBS} -o $@
	@${SIZE} bootst.bug

bootst stboot: bootst.bug ${WRTVID}
	${WRTVID} bootst.bug

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
