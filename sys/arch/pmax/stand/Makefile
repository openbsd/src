#	$NetBSD: Makefile,v 1.7 1996/02/01 22:32:19 mycroft Exp $
#	@(#)Makefile	8.3 (Berkeley) 2/16/94

DESTDIR=
STAND=	../../stand
#VPATH=	${STAND}

# RELOC=80200000 allows for boot prog up to 1D0000 (1900544) bytes long
RELOC=	80200000

S=      ${.CURDIR}/../../..

DEFS= -DSTANDALONE -DSMALL
CFLAGS=	-O2 ${INCPATH} ${DEFS}
AFLAGS=	-O2 ${INCPATH} ${DEFS} -D_LOCORE

.PATH: ${S}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}
.PATH: ${S}/stand ${S}/lib/libsa

#INCPATH=-I. -I/sys
INCPATH=-I${.CURDIR} -I${.CURDIR}/../.. -I${S} -I${S}/lib/libsa

### find out what to use for libkern
.include "$S/lib/libkern/Makefile.inc"
#LIBKERN= ${KERNLIB}
#KERNLIB= ${.CURDIR}/../compile/libkern.a

.include "$S/lib/libsa/Makefile.inc"
LIBSA=  ${SA_LIB}

# not yet: need to write libsa/Makefile.inc first
LIBS=  ${.OBJDIR}/libdrive.a ${.CURDIR}/libsa/libsa.a  ${KERNLIB}
#LIBS=	libdrive.a libsa/libsa.a ../../libkern/obj/libkern.a 

DRIVERS= rz.c
SRCS=	${DRIVERS}
#STUFF=	callvec.c devopen.c getenv.c gets.c strcmp.c
STUFF=

ALL=	boot mkboot mkboottape dec_label rzboot bootrz

.s.o:
	${CPP} -E ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} | \
	    ${AS} -o ${.TARGET}

all: ${ALL}

boot: ${LIBS}

#libsa/libsa.a::
#	cd libsa; make

${.OBJDIR}/libdrive.a: conf.o ${DRIVERS:.c=.o}
	ar crv $@ $?
	ranlib $@

# depend on DEFS

#before other deps on bootconf.o
bootconf.o: conf.o
	rm -f bootconf.c
	ln -s ${.CURDIR}/conf.c bootconf.c
	${CC} -c ${CFLAGS} -DBOOT bootconf.c
	rm -f bootconf.c


# bootable from real disks

boot:	start.o boot.o bootconf.o filesystem.o ${LIBS}
	ld -Map boot.map -N -Ttext ${RELOC} -e start start.o boot.o bootconf.o filesystem.o ${LIBS} -o boot.elf
	elf2aout boot.elf boot

start.o: ${.CURDIR}/start.S

#	${CPP} -E ${CFLAGS:M-[ID]*} -D_LOCORE ${AINC} ${.IMPSRC} | \
#	    ${AS} -o ${.TARGET}

mkboot: ${.CURDIR}/mkboot.c
	${CC} ${CFLAGS} -o mkboot ${.CURDIR}/mkboot.c

mkboottape: ${.CURDIR}/mkboottape.c
	${CC} ${CFLAGS} -o mkboottape ${.CURDIR}/mkboottape.c

dec_label: ${.CURDIR}/dec_label.c
	${CC} ${CFLAGS} -o dec_label ${.CURDIR}/dec_label.c

rzboot bootrz:	mkboot boot
	./mkboot boot rzboot bootrz

# utilities

clean cleandir:
	rm -f .depend *.o *.exe *.i errs make.out core*
	rm -f a.out ${ALL}
	rm -f boot[a-z][a-z] [a-z][a-z]boot
	rm -f libdrive.a
	cd libsa; make cleandir

install:
	./mkboot boot rzboot bootrz
	install -o ${BINOWN} -g ${BINGRP} -m 444 rzboot ${DESTDIR}/usr/mdec
	install -o ${BINOWN} -g ${BINGRP} -m 444 bootrz ${DESTDIR}/usr/mdec

depend: ${SRCS}
	mkdep ${INCPATH} ${DEFS} ${SRCS}
	mkdep -a -p ${INCPATH} ${DEFS} mkboot.c mkboottape.c
	cd libsa; make depend

.include <bsd.dep.mk>
.include <bsd.obj.mk>
