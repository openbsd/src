#	$OpenBSD: Makefile,v 1.2 2003/01/31 20:06:32 drahn Exp $
#	$NetBSD: Makefile,v 1.2 1997/04/17 07:46:24 thorpej Exp $

S=	${.CURDIR}/../../../..
R=../

PROG=		ofwboot
SRCS=		Locore.c boot.c ofdev.c net.c netif_of.c alloc.c cache.c hfs.c
.PATH:		${S}/arch/macppc/stand
SRCS+=		ofwmagic.S
#CFLAGS+=	-DDEBUG -DNETIF_DEBUG
NOMAN=
STRIPFLAG=
BINMODE=	444
OBJCOPY?=	objcopy
SAREL=
BINDIR=         /usr/mdec

NEWVERSWHAT=	"OpenFirmware Boot"

# For now...
RELOC=		20000

ENTRY=		_start

CPPFLAGS+=	-I. -I${.CURDIR}/../../.. -I${.CURDIR}/../../../..
CPPFLAGS+=	-DRELOC=0x${RELOC}
CPPFLAGS+=	-DFIRMWORKSBUGS
CPPFLAGS+=	-DPOWERPC_BOOT_ELF
CPPFLAGS+=	-DXCOFF_GLUE		# for booting PCI Powermacs

LIBS!=          cd $(.CURDIR)/$(R); $(MAKE) libdep

${PROG}: ${OBJS} ${LIBSA} ${LIBZ} ${LIBKERN}
	${LD} -N -X -Ttext ${RELOC} -e ${ENTRY} -o ${PROG} \
	    ${OBJS} ${LIBS}

.include <bsd.prog.mk>
