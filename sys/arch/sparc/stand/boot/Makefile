#	$OpenBSD: Makefile,v 1.20 2013/11/03 21:36:43 deraadt Exp $
#	$NetBSD: Makefile,v 1.2 1995/09/30 21:43:38 pk Exp $

PROG=	boot

NOMAN=	noman
INSTALL_STRIP=
S=	${.CURDIR}/../../../..

.PATH:	${.CURDIR}/../common
SRCS=	srt0.S conf.c dvma.c mmu.c net.c netif_sun.c promdev.c version.c

SRCS+=	boot.c loadfile_sparc.c

.PATH:	${S}/lib/libkern/arch/sparc ${S}/lib/libkern
SRCS+=	bzero.S strlen.c mul.S umul.S strcmp.c sdiv.S urem.S udiv.S rem.S

CFLAGS=	-fno-pie -O2 -I${.CURDIR}/../common -I${.CURDIR}/../../../../arch \
	-I${.CURDIR}/../../../.. -I${.CURDIR}/../../../../lib/libsa ${DEFS}
AFLAGS+=-fno-pie

CLEANFILES+=${PROG}.aout ${PROG}.elf elfclean
LIBS=${LIBSA} ${LIBZ}

elfclean: clean-elf.c
	${HOSTCC} -o elfclean ${.ALLSRC}

${PROG}: ${OBJS} ${LIBS} elfclean
	${LD} -nopie -N -Ttext ${RELOC2} --format a.out-sparc-netbsd \
	    -e start ${OBJS} \
	    ${LIBSA} ${LIBZ} `cc -print-libgcc-file-name` -o ${.TARGET}.elf
	./elfclean ${.TARGET}.elf
	objcopy -j .text -j .data -j .bss -O a.out-sparc-netbsd \
	    ${.TARGET}.elf ${.TARGET}.aout
	(echo -n 01 | tr 01 '\01\03'; tail +3c ${.TARGET}.aout) | \
	    dd obs=512 conv=osync > ${.TARGET}
	@rm ${.TARGET}.aout ${.TARGET}.elf

srt0.o: srt0.S
	${CC} ${CFLAGS} -D_LOCORE -c ${.IMPSRC}

.include <bsd.prog.mk>
