#	$OpenBSD: Makefile,v 1.7 1997/01/15 23:24:15 maja Exp $
#	$NetBSD: Makefile,v 1.11 1996/10/18 06:10:18 thorpej Exp $
#

INCPATH=-I. -I../../.. -I../.. -I../../../lib/libsa

CC=	cc
AS=	as

S=      ../../..
RELOC=	100000
XXRPB=	0F4240
CFLAGS+=-O ${INCPATH} -DSTANDALONE -DRELOC=0x${RELOC} \
	-D_VAX_INLINE_ -DXXRPB=0x$(XXRPB)

DEVS=	autoconf.o hp.o ra.o tmscp.o ctu.o mfm.o rom.o romread.o \
	scsi_low.o scsi_hi.o sd.o

.include "$S/lib/libsa/Makefile.inc"
LIBSA=		${SALIB}

SVAX=	consio.o urem.o udiv.o str.o

all:	${LIBSA} xxboot boot copy edlabel

libsvax.a: ${SVAX}
	ar crv $@ $?
	ranlib $@

urem.o:	../vax/urem.s
	${CC} -x assembler-with-cpp -E ../vax/urem.s | as -o urem.o

udiv.o:	../vax/udiv.s
	${CC} -x assembler-with-cpp -E ../vax/udiv.s | as -o udiv.o

str.o:	str.s
	${CC} -x assembler-with-cpp -E str.s | as -o str.o

# startups

start.o: start.s
	${CC} -x assembler-with-cpp -E start.s | as -o start.o

srt0.o:	srt0.s
	${CC} -x assembler-with-cpp -E srt0.s | as -o srt0.o

# 

xxboot:	start.o bootxx.o romread.o libsvax.a 
	ld -N -Ttext ${RELOC} -o a.out start.o bootxx.o romread.o \
	${LIBSA} libsvax.a
	@strip a.out
	@size a.out
	@dd if=a.out of=xxboot bs=32 skip=1
	@rm -f a.out

boot:	boot.o srt0.o devopen.o conf.o ${DEVS} libsvax.a
	ld -N -Ttext ${RELOC} -e nisse -o $@ srt0.o devopen.o boot.o \
	conf.o ${DEVS} ${LIBSA} libsvax.a
	@strip boot
	@size boot

edlabel: edlabel.o srt0.o devopen.o conf.o ${DEVS} libsvax.a
	ld -N -Ttext ${RELOC} -e nisse -o $@ srt0.o devopen.o edlabel.o\
	conf.o ${DEVS} ${LIBSA} libsvax.a
	@strip edlabel
	@size edlabel

copy:	copy.o srt0.o devopen.o conf.o ${DEVS} libsvax.a
	ld -N -Ttext ${RELOC} -e nisse -o $@ srt0.o devopen.o copy.o \
	conf.o ${DEVS} ${LIBSA} libsvax.a
	@strip copy
	@size copy

#

hp.o:	hp.c
	${CC} -c ${CFLAGS} $*.c

ra.o:	ra.c
	${CC} -c ${CFLAGS} $*.c

autoconf.o:	autoconf.c
	${CC} -c ${CFLAGS} $*.c

conf.o:	conf.c
	${CC} -c ${CFLAGS} $*.c

boot.o:	boot.c
	${CC} -c ${CFLAGS} $*.c

copy.o:	copy.c
	${CC} -c ${CFLAGS} $*.c

romread.o:	romread.s
	${CC} -x assembler-with-cpp -E romread.s | as -o romread.o

init.o:	init.c
	${CC} -c ${CFLAGS} $*.c

bootxx.o: bootxx.c 
	${CC} -c ${CFLAGS} $*.c

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
	copy copy.o init.o

.include <bsd.prog.mk>

