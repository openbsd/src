#	$OpenBSD: Makefile,v 1.10 2002/08/11 23:11:22 art Exp $
#	$NetBSD: Makefile,v 1.2 1995/09/30 21:43:38 pk Exp $

.PATH:	${.CURDIR}/../common

PROG=	boot

NOMAN=	noman
INSTALL_STRIP=

SRCS=	srt0.S boot.c conf.c dvma.c net.c netif_sun.c promdev.c version.c
SRCS+=	loadfile.c

CFLAGS=	-O2 -I${.CURDIR}/../common -I${.CURDIR}/../../../../arch \
	-I${.CURDIR}/../../../.. -I${.CURDIR}/../../../../lib/libsa ${DEFS} \
	-DSPARC_BOOT_AOUT -DSPARC_BOOT_ELF

CLEANFILES+=${PROG}.aout ${PROG}.elf
LIBS=${LIBSA} ${LIBKERN} ${LIBZ}

elfclean: clean-elf.c
	${HOSTCC} -o elfclean ${.ALLSRC}

${PROG}: ${OBJS} ${LIBS} elfclean
	${LD} -N -Ttext ${RELOC2} --format a.out-sparc-netbsd -e start ${OBJS} ${LIBSA} ${LIBKERN} ${LIBZ} `cc -print-libgcc-file-name` -o elf
	./elfclean elf
	objcopy -j .text -j .data -j .bss -O a.out-sparc-netbsd elf a.out
	@size a.out
	(echo -n 01 | tr 01 '\01\03'; tail +3c a.out) > ${.TARGET}
	@rm a.out elf

srt0.o: srt0.S
	${CC} ${CFLAGS} -D_LOCORE -c ${.IMPSRC}

.include <bsd.prog.mk>
