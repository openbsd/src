#	$OpenBSD: Makefile,v 1.5 1997/09/17 10:46:16 downsj Exp $
#	$NetBSD: Makefile,v 1.2 1995/09/30 21:43:38 pk Exp $

.PATH:	${.CURDIR}/../common

PROG=	bootxx

NOMAN=	noman
INSTALL_STRIP=

SRCS=	srt0.S bootxx.c closeall.c dvma.c promdev.c

# pre-built bits of libkern
KOBJS=	__main.o bzero.o urem.o udiv.o
_KOBJS=${KOBJS:S,^,${LIBKERNOBJDIR}/,g}
# pre-built bits of libsa
SOBJS=	alloc.o exit.o printf.o memcpy.o memset.o
_SOBJS=${SOBJS:S,^,${LIBSAOBJDIR}/,g}

CFLAGS= -O2 -I${.CURDIR}/../common -I${.CURDIR}/../../../../arch \
	-I${.CURDIR}/../../../.. -I${.CURDIR}/../../../../lib/libsa -DBOOTXX \
	${DEFS}

${PROG}: ${OBJS}
	${LD} -N -T ${RELOC} -e start ${OBJS} ${_SOBJS} ${_KOBJS}
	# convert to Sun magic
	@size a.out
	(echo -n 01 | tr 01 '\01\03'; tail +3c a.out) > ${.TARGET}
	@rm a.out

srt0.o: srt0.S
	${CC} ${CFLAGS} -D_LOCORE -c ${.IMPSRC}

.include <bsd.prog.mk>
