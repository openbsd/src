#	$NetBSD: Makefile,v 1.2 1995/09/30 21:43:38 pk Exp $

R=	..
.PATH:	${.CURDIR}/${R}
PROG=	boot
SRCS=	srt0.S boot.c promdev.c dvma.c bootnet.c netif_sun.c conf.c version.c
INSTALL_STRIP=
NOMAN=	1
BINDIR=	/usr/mdec
LIBS!=	cd ${.CURDIR}/${R}; ${MAKE} libdep

${PROG}:${OBJS} ${LIBS}
	${LD} -N -T ${RELOC} -e start ${OBJS} ${LIBS}
	# convert to Sun magic
	@size a.out
	(echo -n 01 | tr 01 '\01\03'; tail +3c a.out) > ${.TARGET}
	@rm a.out

.include <bsd.prog.mk>
