#	$OpenBSD: Makefile,v 1.1 1997/05/29 08:22:07 grr Exp $

S=	${.CURDIR}/../../../..
R=	..
.PATH:	${.CURDIR}/${R}
PROG=	zboot
SRCS=	srt0.S boot.c promdev.c dvma.c bootnet.c netif_sun.c conf.c version.c
INSTALL_STRIP=
NOMAN=	1
BINDIR=	/usr/mdec

# include "library" defines and make rules
KERN_AS=	library
KERNDST=	${.CURDIR}/${__objdir}
.include "${S}/lib/libkern/Makefile.inc"
LIBKERN=	${KERNLIB}

Z_AS=		library
ZDST=		${.CURDIR}/${__objdir}
.include "${S}/lib/libz/Makefile.inc"
LIBZ=		${ZLIB}

SA_AS=		library
SADST=		${.CURDIR}/${__objdir}
SAREL=
SA_ZLIB=	yes
.include "${S}/lib/libsa/Makefile.inc"
LIBSA=		${SALIB}

LIBS=	${LIBSA} ${LIBKERN} ${LIBZ}

# ${PROG} overloads the normal rule in bsd.prog.mk

${PROG}:	${OBJS} ${LIBS}
	${LD} -N -T ${RELOC} -e start ${OBJS} \
		-nostdlib -L ${.CURDIR}/${__objdir} -lsa -lkern -lz
	# convert to Sun magic
	@size a.out
	(echo -n 01 | tr 01 '\01\03'; tail +3c a.out) > ${.TARGET}
	@rm a.out

.include <bsd.prog.mk>
