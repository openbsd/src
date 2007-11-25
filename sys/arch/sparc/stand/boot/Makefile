#	$OpenBSD: Makefile,v 1.14 2007/11/25 18:25:33 deraadt Exp $
#	$NetBSD: Makefile,v 1.2 1995/09/30 21:43:38 pk Exp $

.PATH:	${.CURDIR}/../common

PROG=	boot

NOMAN=	noman
INSTALL_STRIP=
S=	${.CURDIR}/../../../..

SRCS=	srt0.S boot.c conf.c dvma.c net.c netif_sun.c promdev.c version.c
SRCS+=	loadfile.c

.PATH:	${S}/lib/libkern/arch/sparc ${S}/lib/libkern
SRCS+=	bzero.S strlen.c mul.S umul.S strcmp.S sdiv.S urem.S udiv.S rem.S

CFLAGS=	-O2 -I${.CURDIR}/../common -I${.CURDIR}/../../../../arch \
	-I${.CURDIR}/../../../.. -I${.CURDIR}/../../../../lib/libsa ${DEFS} \
	-DSPARC_BOOT_AOUT -DSPARC_BOOT_ELF

CLEANFILES+=${PROG}.aout ${PROG}.elf elfclean
LIBS=${LIBSA} ${LIBZ}

elfclean: clean-elf.c
	${HOSTCC} -o elfclean ${.ALLSRC}

${PROG}: ${OBJS} ${LIBS} elfclean
	${LD} -N -Ttext ${RELOC2} --format a.out-sparc-netbsd -e start ${OBJS} ${LIBSA} ${LIBKERN} ${LIBZ} `cc -print-libgcc-file-name` -o ${.TARGET}.elf
	./elfclean ${.TARGET}.elf
	objcopy -j .text -j .data -j .bss -O a.out-sparc-netbsd \
	    ${.TARGET}.elf ${.TARGET}.aout
	@size ${.TARGET}.aout
	(echo -n 01 | tr 01 '\01\03'; tail +3c ${.TARGET}.aout) | \
	    dd obs=512 conv=osync > ${.TARGET}
	@rm ${.TARGET}.aout ${.TARGET}.elf

srt0.o: srt0.S
	${CC} ${CFLAGS} -D_LOCORE -c ${.IMPSRC}

.include <bsd.prog.mk>
