#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	     $Id: Makefile,v 1.2 1995/12/06 10:54:54 deraadt Exp $

RELOC=0x3F0000 

S=	${.CURDIR}/../../../..
DEFS= -DSTANDALONE -DCOMPAT_NOLABEL # -DROMPRF
INCPATH=-I${.CURDIR} -I${.CURDIR}/../../include -I${S} -I${S}/lib/libsa
CFLAGS=	-O2 ${INCPATH} ${DEFS} ${COPTS}
CLEANFILES+=sdboot bootsd bootsd.bug

#.PATH: ${S}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}
#.PATH: ${S}/lib/libsa

.include "${S}/arch/${MACHINE}/stand/libsa/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/libbug/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/bugcrt/Makefile.inc"
.include "${S}/arch/${MACHINE}/stand/wrtvid/Makefile.inc"

SRCS=   boot.c filesystem.c bugdev.c version.c

LIBS=	${LIBSA} ${LIBBUG}

OBJS= ${SRCS:N*.h:R:S/$/.o/g}

BOOTS=	bootsd sdboot
ALL=	${BOOTS} 

all: ${ALL}

devopen.o machdep.o: Makefile

bootsd.bug:	${OBJS} ${BUGCRT} ${LIBS}
	${LD} -N -T ${RELOC} ${BUGCRT} ${OBJS} ${LIBS} -o $@
	@size bootsd.bug

bootsd sdboot: bootsd.bug ${WRTVID}
	cp bootsd.bug bootsd.bin
	strip bootsd.bin
	@if [ `size bootsd.bin | awk 'BEGIN {getline} {print $$1+$$2;}'` -gt 7168 ];\
	then\
		echo BOOTBLOCKS ARE TOO BIG;\
		fail;\
	fi
	${WRTVID} bootsd.bin

install:
	install -c -m 555 -g bin -o bin ${BOOTS} ${DESTDIR}${MDEC_DIR}

.include <bsd.prog.mk>
