#	$OpenBSD: Makefile,v 1.11 1997/07/13 07:21:43 downsj Exp $
#	$NetBSD: Makefile,v 1.22 1996/10/18 06:03:25 thorpej Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/10/93

NOPROG=	noprog
NOMAN=	noman

#SUBDIR=	libkern libsa

# RELOC=FFF00000 allows for boot prog up to FF000 (1044480) bytes long
RELOC=	FFF00000

CONS=	-DDCACONSOLE -DITECONSOLE -DDCMCONSOLE -DAPCICONSOLE
CONS+=	-DHIL_KEYBOARD -DUK_KEYBOARD -DDOMAIN_KEYBOARD
DEFS=	-DSTANDALONE ${CONS} -Dhp300
#DEFS+=	-DNETIF_DEBUG -DRPC_DEBUG -DNFS_DEBUG -DRARP_DEBUG -DNET_DEBUG
#DEFS+=	-DLE_DEBUG
CFLAGS=	-O3 -msoft-float ${INCPATH} ${DEFS}

SRCS=	apci.c autoconf.c cons.c ct.c dca.c dcm.c devopen.c dnkbd.c fhpib.c \
	hd.c hil.c hpib.c if_le.c ite.c ite_dv.c ite_gb.c ite_rb.c \
	ite_subr.c ite_tc.c ite_hy.c kbd.c kbdconf.c kbdvar.h machdep.c \
	nhpib.c prf.c scsi.c sd.c version.c
OBJS=	${SRCS:N*.h:R:S/$/.o/g}

S=	${.CURDIR}/../../..

.PATH: ${S}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}
.PATH: ${S}/stand

INCPATH=-I${.CURDIR} -I${.CURDIR}/../.. -I${S} -I${S}/lib/libsa

.include "${.CURDIR}/libkern/Makefile.inc"
LIBKERN=	${KERN_LIB}

### find out what to use for libz
Z_AS=	library
.include "${S}/lib/libz/Makefile.inc"
LIBZ=	${ZLIB}

.include "${.CURDIR}/libsa/Makefile.inc"
LIBSA=	${SA_LIB}

LIBS=	${OBJS} ${LIBSA} ${LIBZ} ${LIBKERN}

BOOTS=	uboot.lif
BOOTAOUTS=uboot
ALL=	${BOOTS} mkboot installboot

all: ${ALL}

${BOOTS}: ${LIBS}

# depend on CFLAGS

${OBJS}: Makefile

# startups

srt0.o: ${.CURDIR}/srt0.s
	${CC} ${INCPATH} ${DEFS} -c ${.CURDIR}/srt0.s

# unified boot program (disk, network, tape)
uboot.lif: uboot mkboot
	./mkboot uboot $@

uboot:	srt0.o uboot.o tgets.o netio.o clock.o conf.o ${LIBS}
	${LD} -N -T ${RELOC} -e begin srt0.o uboot.o tgets.o netio.o clock.o \
	    conf.o ${LIBS} -o $@
	@size $@
	@echo $@ total size should not exceed 1044480 bytes

# helper program ... turns OMAGIC into LIF
mkboot: ${.CURDIR}/mkboot.c
	${CC} ${CFLAGS} ${.CURDIR}/mkboot.c -o $@

installboot: ${.CURDIR}/installboot.sh
	@rm -f installboot
	cp -p ${.CURDIR}/installboot.sh installboot

# utilities

clean::
	rm -f *.o *.i
	rm -f a.out ${BOOTS} ${BOOTAOUTS}
	rm -f mkboot installboot

install:
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	    ${DESTDIR}/usr/mdec/rbootd
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 installboot \
	    ${DESTDIR}/usr/mdec
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 uboot.lif \
	    ${DESTDIR}/usr/mdec
	rm -f ${DESTDIR}/usr/mdec/hdboot
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/hdboot
	rm -f ${DESTDIR}/usr/mdec/boothd
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/boothd
	rm -f ${DESTDIR}/usr/mdec/sdboot
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/sdboot
	rm -f ${DESTDIR}/usr/mdec/bootsd
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/bootsd
	rm -f ${DESTDIR}/usr/mdec/ctboot
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/ctboot
	rm -f ${DESTDIR}/usr/mdec/bootct
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/bootct
	rm -f ${DESTDIR}/usr/mdec/rbootd/SYS_UBOOT
	ln ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/usr/mdec/rbootd/SYS_UBOOT

#obj: _SUBDIRUSE

.include <bsd.prog.mk>
