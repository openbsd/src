#	$NetBSD: Makefile,v 1.7.2.2 1995/10/24 19:23:09 phil Exp $

#	@(#)Makefile	8.1 (Berkeley) 6/10/93

#DESTDIR=
STANDDIR=${DESTDIR}/stand
# load at 0x400000 - 30 * 1024 - 0x10000
# that way the boot code will be at the correct
# address if the floppy image is started from memory
# NOTE: alloc() function takes memory after _end
RELOC=	3E8800

CONS= -DSCNCONSOLE
DEFS= -DSTANDALONE ${CONS}
CFLAGS=	-O ${INCPATH} ${DEFS} -fwritable-strings

SRCS=   cons.c devopen.c scn.c scsi_low.c scsi_hi.c \
	conf.c prf.c tgets.c machdep.c sd.c rd.c filesystem.c

S=	${.CURDIR}/../../..
SAREL=
KERNREL=

.PATH: ${S}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}
.PATH: ${S}/stand ${S}/lib/libsa

INCPATH=-I${.CURDIR} -I${.CURDIR}/../.. -I${S} -I${S}/lib/libsa

### find out what to use for libs
.include "../../../lib/libkern/Makefile.inc"
LIBKERN=	${KERNLIB}

NO_NET=
.include "../../../lib/libsa/Makefile.inc"
LIBSA=	${SALIB}

#LIBS=	${LIBSA} ${.OBJDIR}/libdrive.a \
#	${LIBKERN} ${LIBSA} ${LIBKERN}
LIBS=	${LIBSA} ${.OBJDIR}/libdrive.a \
	${LIBKERN} ${LIBSA}

BOOTS=	boot zboot.o
ALL=	${BOOTS}

all: ${ALL}

${BOOTS}: ${LIBS}

OBJS=	${SRCS:N*.h:R:S/$/.o/g}
${.OBJDIR}/libdrive.a: ${OBJS}
	ar crv $@ $?
	ranlib $@

# depend on DEFS

devopen.o machdep.o srt0.o: Makefile
cons.o: Makefile

# startups

srt0.o: ${.CURDIR}/srt0.s
	cpp ${INCPATH} ${DEFS} ${.CURDIR}/srt0.s | as -o srt0.o

# new boot
boot:	boot.o srt0.o ${LIBS}
	ld -z -T ${RELOC} -e begin srt0.o boot.o ${LIBS} -o $@
	@size boot

zboot.o:	inflate.o
	ld -r srt0.o inflate.o $(LIBS) -o zboot.o

# new boot user mode test
TESTBOOT=	boot.o test.o $(SC) ${LIBS}
# objects from regular libc;
SC=	cerror.o syscall.o malloc.o sbrk.o getpagesize.o
testboot:	$(TESTBOOT)
	ld -o testboot /usr/lib/crt0.o $(TESTBOOT) ${LIBS} -o $@

$(SC): /usr/lib/libc.a
	ar x /usr/lib/libc.a $(SC)

# utilities

install: ${ALL}
	cp ${ALL} ${DESTDIR}/usr/mdec

.include <bsd.dep.mk>
.include <bsd.obj.mk>
.include <bsd.subdir.mk>

FRC:

# clean ...

clean::
	rm -f ${ALL} ${.OBJDIR}/libdrive.a ${OBJS} boot.o srt0.o inflate.o

cleandir: clean
