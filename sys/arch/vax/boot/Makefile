#	$NetBSD: Makefile,v 1.5 1995/10/20 13:35:44 ragge Exp $
#

INCPATH=-I. -I../../.. -I../.. -I../../../lib/libsa

CC=	cc
AS=	as

S=      ../../..
RELOC=	100000
CFLAGS+=-O ${INCPATH} -DSTANDALONE -DRELOC=0x${RELOC} -D_VAX_INLINE_

DEVS=	autoconf.o hp.o ra.o tmscp.o

.include "$S/lib/libkern/Makefile.inc"
LIBKERN=        ${KERNLIB}

.include "$S/lib/libsa/Makefile.inc"
LIBSA=		${SALIB}

all:	xxboot boot copy edlabel

libsvax.a: consio.o urem.o udiv.o
	ar crv $@ $?
	ranlib $@

urem.o:	../vax/urem.s
	${CC} -x assembler-with-cpp -E ../vax/urem.s | as -o urem.o

udiv.o:	../vax/udiv.s
	${CC} -x assembler-with-cpp -E ../vax/udiv.s | as -o udiv.o

# startups

start.o: start.s
	${CC} -x assembler-with-cpp -E start.s | as -o start.o

srt0.o:	srt0.s
	${CC} -x assembler-with-cpp -E srt0.s | as -o srt0.o

# 

xxboot:	start.o bootxx.o romread.o ${LIBSA} ${LIBKERN} libsvax.a 
	ld -N -Ttext ${RELOC} -o a.out start.o bootxx.o romread.o \
	${LIBSA} ${LIBKERN} libsvax.a
	@strip a.out
	@size a.out
	@dd if=a.out of=xxboot bs=32 skip=1
	@rm -f a.out

boot:	boot.o srt0.o devopen.o conf.o ${DEVS} ${LIBKERN} ${LIBSA} libsvax.a
	ld -N -Ttext ${RELOC} -e nisse -o $@ srt0.o devopen.o boot.o \
	conf.o ${DEVS} ${LIBSA} ${LIBKERN} libsvax.a
	@strip boot
	@size boot

edlabel: edlabel.o srt0.o devopen.o conf.o ${DEVS} ${LIBKERN} ${LIBSA} libsvax.a
	ld -N -Ttext ${RELOC} -e nisse -o $@ srt0.o devopen.o edlabel.o\
	conf.o ${DEVS} ${LIBSA} ${LIBKERN} libsvax.a
	@strip edlabel
	@size edlabel

copy:	copy.o srt0.o devopen.o conf.o ${DEVS} ${LIBKERN} ${LIBSA} libsvax.a
	ld -N -Ttext ${RELOC} -e nisse -o $@ srt0.o devopen.o copy.o \
	conf.o ${DEVS} ${LIBSA} ${LIBKERN} libsvax.a
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

clean::
	rm -f start.o romread.o bootxx.o init.o xxboot boot racopy \
	libsvax.a udiv.o urem.o consio.o ${DEVS} edlabel edlabel.o
	rm -f conf.o boot.o rom.o racopy.o srt0.o devopen.o rootcopy.o \
	copy copy.o

