#	$NetBSD: Makefile,v 1.6 1996/02/01 22:31:28 mycroft Exp $
#	@(#)Makefile	7.9 (Berkeley) 5/8/91

S	= ${.CURDIR}/../../..
I386	= ${.CURDIR}/..
CSH	= csh
RMHDR	= ${CSH} ${.CURDIR}/../boot/rmaouthdr
STAND=	/sys/stand
INCPATH=-I${.CURDIR} -I${S} -I${S}/arch -I${S}/ufs -I${S} -I${STAND}
VPATH=	${STAND}
STANDDIR= ${DESTDIR}/usr/mdec

AFLAGS+=	-v -Wa,-a -D_LOCORE
CFLAGS+=	-m386 -O6 -D_STANDALONE -DAT386 -O ${INCPATH}
#CPPFLAGS+=	${INCPATH} -D_KERNEL -DRELOC=0x${RELOC}
CPPFLAGS+=	${INCPATH} -DRELOC=0x${RELOC}
LDADD+=		${LIBC}

RELOC=	98000
RELOC2=	98200

DRIVERS=cga.c fd.c kbd.c wd.c sd.c cd.c
ZLIB=	zlib.c
BOOTS=	fdbootblk.S wdbootblk.S sdbootblk.c cdbootblk.S
SRCS=	boot.c prf.c srt0.S ${BOOTS} ${ZLIB} ${DRIVERS}

CLEANFILES+=	*.o conf[fwsc]d.c bread[fwsc]d.c \
		[fwsc]dboot boot[fwsc]d boot[fwsc]d.sym boot[fwsc]d.c

WDOBJS=	wsrt0.o boot.o bmap.o cga.o fs.o kbd.o prf.o wd.o breadwd.o clock.o
FDOBJS=	wsrt0.o boot.o bmap.o cga.o fs.o kbd.o prf.o fd.o breadfd.o clock.o
SDOBJS=	wsrt0.o boot.o bmap.o cga.o fs.o kbd.o prf.o sd.o breadsd.o clock.o
CDOBJS=	wsrt0.o boot.o bmap.o cga.o fs.o kbd.o prf.o cd.o breadcd.o clock.o

.PATH: ${I386}/isa ${S}/net

ALL=	wdboot bootwd fdboot bootfd # sdboot bootsd cdboot bootcd
all:	${ALL}

# startups

wsrt0.o: ${.CURDIR}/srt0.S
	${CC} -c ${CPPFLAGS} ${AFLAGS} -DREL -DSMALL -o ${.TARGET} ${.CURDIR}/srt0.S

relsrt0.o: ${.CURDIR}/srt0.c
	${CC} -c ${CFLAGS} -DREL -o ${.TARGET} ${.CURDIR}/srt0.S

# getting booted from disc

wdboot: wdbootblk.o
	${LD} -N -T ${RELOC} wdbootblk.o
	rm -f ${.TARGET}
	strip a.out
	${RMHDR} a.out ${.TARGET}
	rm -f a.out
	ls -l ${.TARGET}

bootwd.sym bootwd:	${WDOBJS}
	${LD} -T ${RELOC2} ${WDOBJS} ${LDADD}
	cp a.out bootwd.sym
	size a.out
	rm -f $@; strip a.out; ${RMHDR} a.out $@; rm -f a.out; ls -l $@

fdboot: fdbootblk.o
	${LD} -N -T ${RELOC} fdbootblk.o
	rm -f $@; strip a.out; ${RMHDR} a.out $@; rm -f a.out; ls -l $@

bootfd.sym bootfd:	${FDOBJS}
	${LD} -N -T ${RELOC2} ${FDOBJS} ${LDADD}
	cp a.out bootfd.sym
	size a.out
	rm -f $@; strip a.out; ${RMHDR} a.out $@; rm -f a.out; ls -l $@

sdboot:	sdbootblk.o
	${LD} -N -T 7c00 sdbootblk.o
	rm -f $@; strip a.out; ${RMHDR} a.out $@; rm -f a.out; ls -l $@

bootsd.sym bootsd:	${SDOBJS}
	${LD} -N -T ${RELOC2} ${SDOBJS} ${LDADD}
	cp a.out bootsd.sym
	size a.out
	rm -f $@; strip a.out; ${RMHDR} a.out $@; rm -f a.out; ls -l $@

breadwd.o: breadwd.c breadxx.o
breadfd.o: breadfd.c breadxx.o
breadsd.o: breadsd.c breadxx.o

breadxx.o:
	touch breadxx.o

breadwd.c: ${.CURDIR}/breadxx.c
	@rm -f breadwd.c
	sed -e 's/XX/wd/' -e 's/xx/wd/g' < ${.CURDIR}/breadxx.c >> breadwd.c

breadfd.c: ${.CURDIR}/breadxx.c
	rm -f breadfd.c
	sed -e 's/XX/fd/' -e 's/xx/fd/g' < ${.CURDIR}/breadxx.c >> breadfd.c

breadsd.c: ${.CURDIR}/breadxx.c
	rm -f breadsd.c
	sed -e 's/XX/as/' -e 's/xx/as/g' < ${.CURDIR}/breadxx.c >> breadsd.c

#clean:

.MAKE:  make.out                      
make.out::
	rm -f ${OBJS}
	${MAKE} ${.MAKEFLAGS} ${OBJS} > make.out


install: ${ALL}
	install -c ${ALL} ${STANDDIR}

.include <bsd.prog.mk>

