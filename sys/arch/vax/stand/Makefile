#	$OpenBSD: Makefile,v 1.9 1998/05/11 07:39:13 niklas Exp $
#	$NetBSD: Makefile,v 1.14 1997/06/29 21:30:09 ragge Exp $
#

S!=	cd ${.CURDIR}/../../..; pwd
OBJ!=	pwd

AR?=	ar
AS?=	as
CC?=	cc
LD?=	ld
RANLIB?=ranlib
SIZE?=	size
STRIP?=	strip

BINOWN=	bin
BINGRP=	bin

INCPATH=-nostdinc -I${OBJ} -I${.CURDIR} -I${.CURDIR}/.. -I${S} -I${S}/lib/libsa
RELOC=	100000
XXRPB=	0F4240
CFLAGS+=-O ${INCPATH} -D_STANDALONE -DRELOC=0x${RELOC} -DXXRPB=0x$(XXRPB)

DEVS=	autoconf.o hp.o ra.o tmscp.o ctu.o mfm.o rom.o romread.o \
	scsi_low.o scsi_hi.o sd.o netio.o if_le.o

SAREL=
.include "$S/lib/libsa/Makefile.inc"
LIBS=	-L. -Llib/sa -lsvax -lsa -lsvax


SVAX=	consio.o urem.o udiv.o str.o

all:	${LIBSA} xxboot boot copy edlabel

includes:

libsvax.a: ${SVAX}
	${AR} crv $@ $?
	${RANLIB} $@

urem.o:	../vax/urem.s
	${CC} -x assembler-with-cpp -E ${.CURDIR}/../vax/urem.s | \
	    ${AS} -o urem.o

udiv.o:	../vax/udiv.s
	${CC} -x assembler-with-cpp -E ${.CURDIR}/../vax/udiv.s | \
	    ${AS} -o udiv.o

str.o:	str.s
	${CC} -x assembler-with-cpp -E ${.CURDIR}/str.s | ${AS} -o str.o

# startups

start.o: start.s
	${CC} ${CFLAGS} -x assembler-with-cpp -E ${.CURDIR}/start.s | \
	    ${AS} -o start.o

srt0.o:	srt0.s
	${CC} -x assembler-with-cpp -E ${.CURDIR}/srt0.s | ${AS} -o srt0.o

# 

xxboot:	start.o bootxx.o romread.o libsvax.a ${SALIB}
	${LD} -N -Ttext ${RELOC} -o a.out start.o bootxx.o romread.o ${LIBS}
	@${STRIP} a.out
	@${SIZE} a.out
	@dd if=a.out of=xxboot bs=32 skip=1
	@rm -f a.out

boot:	boot.o srt0.o devopen.o conf.o ${DEVS} libsvax.a ${SALIB}
	@date '+_vers: .globl _vers; .asciz ">> NetBSD/vax boot [%y%m%d %H:%M] <<"' | ${AS} -o vers.o
	${LD} -N -Ttext ${RELOC} -e nisse -o $@ srt0.o devopen.o boot.o \
	conf.o ${DEVS} ${LIBS} vers.o
	@${STRIP} boot
	@${SIZE} boot

edlabel: edlabel.o srt0.o devopen.o conf.o ${DEVS} libsvax.a
	@date '+_vers: .globl _vers; .asciz ">> NetBSD/vax boot [%y%m%d %H:%M] <<"' | ${AS} -o vers.o
	${LD} -N -Ttext ${RELOC} -e nisse -o $@ srt0.o devopen.o edlabel.o\
	conf.o ${DEVS} ${LIBS} vers.o
	@${STRIP} edlabel
	@${SIZE} edlabel

copy:	copy.o srt0.o devopen.o conf.o ${DEVS} libsvax.a
	@date '+_vers: .globl _vers; .asciz ">> NetBSD/vax boot [%y%m%d %H:%M] <<"' | ${AS} -o vers.o
	${LD} -N -Ttext ${RELOC} -e nisse -o $@ srt0.o devopen.o copy.o \
	conf.o ${DEVS} ${LIBS} vers.o
	@${STRIP} copy
	@${SIZE} copy

#

romread.o:	romread.s
	${CC} -x assembler-with-cpp -E ${.CURDIR}/romread.s | \
	    ${AS} -o romread.o

#
install: boot xxboot
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 boot ${DESTDIR}/
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 xxboot \
	    ${DESTDIR}/usr/mdec
	rm -f ${DESTDIR}/usr/mdec/raboot
	ln ${DESTDIR}/usr/mdec/xxboot ${DESTDIR}/usr/mdec/raboot
	rm -f ${DESTDIR}/usr/mdec/hpboot
	ln ${DESTDIR}/usr/mdec/xxboot ${DESTDIR}/usr/mdec/hpboot

clean::
	rm -f start.o romread.o bootxx.o init.o xxboot boot racopy \
	libsvax.a udiv.o urem.o consio.o ${DEVS} edlabel edlabel.o
	rm -f conf.o boot.o rom.o racopy.o srt0.o devopen.o rootcopy.o \
	copy copy.o init.o str.o vers.o

.include <bsd.prog.mk>

