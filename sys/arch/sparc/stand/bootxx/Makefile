#	$NetBSD: Makefile,v 1.5 1995/10/10 20:07:54 pk Exp $

R=	..
.PATH:	${.CURDIR}/${R}
PROG=	bootxx
SRCS=	srt0.S bootxx.c promdev.c dvma.c
KOBJS=	__main.o bzero.o urem.o udiv.o
NOMAN=	1
INSTALL_STRIP=
BINDIR=	/usr/mdec
CFLAGS= -DBOOTXX

LIBS!=	cd ${.CURDIR}/${R}; ${MAKE} sadep
KOBJDIR!= cd ${.CURDIR}/${R}; ${MAKE} kernlibdir
_KOBJS=${KOBJS:S,^,${KOBJDIR}/,g}

${PROG}:${OBJS} ${_KOBJS} ${LIBS}
	${LD} -N -T ${RELOC} -e start ${OBJS} ${_KOBJS} ${LIBS}
	# convert to Sun magic
	@size a.out
	(echo -n 01 | tr 01 '\01\03'; tail +3c a.out) > ${.TARGET}
	@rm a.out

.include <bsd.prog.mk>
