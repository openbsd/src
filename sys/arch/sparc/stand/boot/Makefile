#	$OpenBSD: Makefile,v 1.9 2000/02/19 15:38:24 deraadt Exp $
#	$NetBSD: Makefile,v 1.2 1995/09/30 21:43:38 pk Exp $

.PATH:	${.CURDIR}/../common

PROG=	boot

NOMAN=	noman
INSTALL_STRIP=

SRCS=	srt0.S boot.c conf.c dvma.c net.c netif_sun.c promdev.c version.c

CFLAGS=	-O2 -I${.CURDIR}/../common -I${.CURDIR}/../../../../arch \
	-I${.CURDIR}/../../../.. -I${.CURDIR}/../../../../lib/libsa ${DEFS}

${PROG}: ${OBJS} ${LIBS}
	${LD} -N -T ${RELOC2} -e start ${OBJS} ${LIBSA} ${LIBKERN} ${LIBZ} `cc -print-libgcc-file-name`
	# convert to Sun magic
	@size a.out
	(echo -n 01 | tr 01 '\01\03'; tail +3c a.out) > ${.TARGET}
	@rm a.out

srt0.o: srt0.S
	${CC} ${CFLAGS} -D_LOCORE -c ${.IMPSRC}

.include <bsd.prog.mk>
