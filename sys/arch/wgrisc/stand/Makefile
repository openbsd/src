#	$NetBSD: Makefile,v 1.5 1995/01/18 06:53:36 mellon Exp $
#	@(#)Makefile	8.3 (Berkeley) 2/16/94

DESTDIR=
STAND=	../../stand
#VPATH=	${STAND}

# RELOC=80200000 allows for boot prog up to 1D0000 (1900544) bytes long
RELOC=	80200000

S=      ../../..
INCLUDES=       -I. -I$S/arch -I$S -I$S/lib/libsa
CPPFLAGS=       ${INCLUDES} ${IDENT} -D_KERNEL -Dwgrisc
CDIAGFLAGS=     -Werror -Wall -Wno-uninitialized -Wno-format


DEFS= -DSTANDALONE -DDEBUG
CFLAGS=	${DEBUG} -O2 ${CDIAGFLAGS} -mno-abicalls -mips1 -mcpu=r3000
AFLAGS=	-O2 ${INCPATH} ${DEFS} -DLOCORE

### find out what to use for libkern
.include "$S/lib/libkern/Makefile.inc"
LIBKERN= ${KERNLIB}

.include "$S/lib/libsa/Makefile.inc"

# not yet: need to write libsa/Makefile.inc first
LIBS=  ${KERNLIB} ${SALIB}

DRIVERS= rz.c
SRCS=	${DRIVERS}
#STUFF=	callvec.c devopen.c getenv.c gets.c strcmp.c
STUFF=

ALL=	boot

.s.o:
	${CPP} -E ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} | \
	    ${AS} -o ${.TARGET}

all: ${ALL}

boot: ${LIBS}

#libsa/libsa.a::
#	cd libsa; make

${.OBJDIR}/libdrive.a: ${DRIVERS:.c=.o}
	ar crv $@ $?
	ranlib $@

# depend on DEFS


# bootable from real disks

boot:	start.o boot.o filesystem.o ${LIBS}
	ld -N -Ttext ${RELOC} -e __start start.o boot.o filesystem.o ${LIBS} -o boot.elf
	elf2ecoff boot.elf boot

start.o: ${.CURDIR}/start.S

mkboot: ${.CURDIR}/mkboot.c
	${CC} ${CFLAGS} -o mkboot ${.CURDIR}/mkboot.c

# utilities

clean::
	rm -f .depend *.o *.exe *.i errs make.out core*
	rm -f a.out ${ALL}
	rm -f libdrive.a
	cd libsa; make cleandir

install:

.include <bsd.dep.mk>
.include <bsd.obj.mk>
