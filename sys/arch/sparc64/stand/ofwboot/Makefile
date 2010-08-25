#	$OpenBSD: Makefile,v 1.16 2010/08/25 12:53:38 jsing Exp $
#	$NetBSD: Makefile,v 1.2 2001/03/04 14:50:05 mrg Exp $

CURDIR=	${.CURDIR}
S=	${CURDIR}/../../../..

#
# Override normal settings
#

WARNS=		0

PROG?=		ofwboot
NOMAN=		ja, man!

.PATH:		${S}/arch/sparc64/sparc64
SRCS=		srt0.s Locore.c alloc.c boot.c elf64_exec.c net.c netif_of.c \
		ofdev.c vers.c

.PATH:		${S}/lib/libkern/arch/sparc64 ${S}/lib/libkern
SRCS+=		strlcpy.c strcmp.c strlcat.c strlen.c

CWARNFLAGS+=	-Wno-main
AFLAGS+=	-Wa,-Av9a
AFLAGS+=	-x assembler-with-cpp -D_LOCORE -D__ELF__
CFLAGS+=	${COPTS}
CPPFLAGS+=	-D_STANDALONE -DSUN4U -nostdinc
#CPPFLAGS+=	-DNETIF_DEBUG 

BINMODE=	444

NEWVERSWHAT=	"OpenFirmware Boot"

#
# ELF64 defaults to 1MB
#
RELOC=		100000

ENTRY=		_start

CLEANFILES+=	sparc machine

CPPFLAGS+=	-I${CURDIR}/../../.. -I${CURDIR}/../../../.. -I${CURDIR} -I.
CPPFLAGS+=	-DRELOC=0x${RELOC}

#
# XXXXX FIXME
#
CPPFLAGS+=	-DSPARC_BOOT_UFS
#CPPFLAGS+=	-DSPARC_BOOT_HSFS

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../include machine)
.endif

${PROG}: ${OBJS} ${LIBSA} ${LIBZ}
	${LD} -N -Ttext ${RELOC} -e ${ENTRY} -o ${PROG} \
	    ${OBJS} -L${LIBSADIR} ${LIBSA} \
	    -L${LIBZDIR} ${LIBZ}

NORMAL_S=	${CC} ${AFLAGS} ${CPPFLAGS} -c $<
srt0.o: srt0.s
	${NORMAL_S}

.include <bsd.prog.mk>
