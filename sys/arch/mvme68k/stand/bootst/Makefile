#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$OpenBSD: Makefile,v 1.16 2013/11/06 19:37:06 miod Exp $

STRIP?=	strip

S=	${.CURDIR}/../../../..
DEFS=	-DSTANDALONE -DCOMPAT_NOLABEL
INCPATH=-I${.CURDIR} -I${.CURDIR}/../libsa -I${.CURDIR}/../libbug \
	-I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS+=-O2 ${INCPATH} ${DEFS} ${COPTS}
LDFLAGS=-N -T ${S}/arch/mvme68k/stand/ld.script -Ttext ${STAGE2_RELOC}
NOPIE=
CLEANFILES+=stboot bootst bootst.bug bootst.strip

.include "${S}/arch/mvme68k/stand/wrtvid/Makefile.inc"
.include "${S}/arch/mvme68k/stand/bugcrt/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libbug/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libsa/Makefile.inc"
.include "${S}/arch/mvme68k/stand/libz/Makefile.inc"

SRCS=   boot.c conf.c dev_tape.c rawfs.c version.c

LIBS=	${LIBSA} ${LIBBUG} ${LIBZ}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootst stboot
ALL=	${BOOTS} 

all: ${ALL}

bootst.bug: ${BUGCRT} ${OBJS} ${LIBS}
	${LD} ${LDFLAGS} -o $@ \
	    ${BUGCRT} ${OBJS} ${LIBS} `${CC} -print-libgcc-file-name`

bootst stboot: bootst.bug ${WRTVID}
	${STRIP} -I a.out-m68k-netbsd -o bootst.strip bootst.bug
	${WRTVID} bootst.strip

install: bootst stboot
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
