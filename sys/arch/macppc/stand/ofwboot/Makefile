#	$OpenBSD: Makefile,v 1.5 2007/11/25 18:25:32 deraadt Exp $
#	$NetBSD: Makefile,v 1.2 1997/04/17 07:46:24 thorpej Exp $

S=	${.CURDIR}/../../../..
R=../

PROG=		ofwboot
SRCS=		Locore.c main.c ofdev.c net.c netif_of.c alloc.c cache.c hfs.c
SRCS+=		boot.c conf.c cmd.c vars.c ctime.c strtol.c
.PATH:		${S}/arch/macppc/stand
SRCS+=		ofwmagic.S
#CFLAGS+=	-DDEBUG -DNETIF_DEBUG
NOMAN=
STRIPFLAG=
BINMODE=	444
OBJCOPY?=	objcopy
SAREL=
BINDIR=         /usr/mdec

.PATH:          ${S}/stand/boot
.PATH:          ${S}/lib/libsa

.PATH:		${S}/lib/libkern/arch/powerpc ${S}/lib/libkern
SRCS+=		strlcpy.c memcpy.c strlen.c strrchr.c strlcat.c ashrdi3.c

NEWVERSWHAT=	"OpenFirmware Boot"

# For now...
RELOC=		20000

ENTRY=		_start

CPPFLAGS+=	-I. -I${.CURDIR}/../../.. -I${.CURDIR}/../../../..
CPPFLAGS+=	-DRELOC=0x${RELOC} -DCONSPEED=57600

LIBS!=          cd $(.CURDIR)/$(R); $(MAKE) libdep

${PROG}: ${OBJS} ${LIBSA} ${LIBZ}
	${LD} -N -X -Ttext ${RELOC} -e ${ENTRY} -o ${PROG} \
	    ${OBJS} ${LIBS}

.include <bsd.prog.mk>
