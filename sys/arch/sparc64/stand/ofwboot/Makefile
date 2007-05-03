#	$OpenBSD: Makefile,v 1.9 2007/05/03 18:38:11 deraadt Exp $
#	$NetBSD: Makefile,v 1.2 2001/03/04 14:50:05 mrg Exp $

CURDIR=	${.CURDIR}
S=	${CURDIR}/../../../..

#
# Override normal settings
#

WARNS=		0

PROG?=		ofwboot
NOMAN=		ja, man!
SRCS=		srt0.s Locore.c boot.c ofdev.c alloc.c net.c netif_of.c vers.c
.PATH:		${S}/arch/sparc64/sparc64

CWARNFLAGS+=	-Wno-main
AFLAGS+=	-Wa,-Av9a
AFLAGS+=	-x assembler-with-cpp -traditional-cpp -D_LOCORE -D__ELF__
CFLAGS+=	${COPTS}
CPPFLAGS+=	-D_STANDALONE -DSUN4U -nostdinc
#CPPFLAGS+=	-DNETIF_DEBUG 

BINMODE=	444

NEWVERSWHAT=	"OpenFirmware Boot"

#
# ELF64 defaults to 1MB
#
# We may get address conflicts with other bootloaders, say
# Sun's ufsboot, so we'll pick a reasonably empty address.
#
RELOC=		800000

ENTRY=		_start

CLEANFILES+=	vers.c sparc machine

CPPFLAGS+=	-I${CURDIR}/../../.. -I${CURDIR}/../../../.. -I${CURDIR} -I.
CPPFLAGS+=	-DRELOC=0x${RELOC}

#
# XXXXX FIXME
#
CPPFLAGS+=	-DSPARC_BOOT_ELF
CPPFLAGS+=	-DSPARC_BOOT_UFS
CPPFLAGS+=	-DSPARC_BOOT_NFS
#CPPFLAGS+=	-DSPARC_BOOT_HSFS

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(kernlibdir) && !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../include machine)
.endif

${PROG}: ${OBJS} ${LIBSA} ${LIBZ} ${LIBKERN}
	${LD} -N -Ttext ${RELOC} -e ${ENTRY} -o ${PROG} \
	    ${OBJS} -L${LIBSADIR} ${LIBSA} -L${LIBKERNDIR} ${LIBKERN} \
	    -L${LIBZDIR} ${LIBZ}

NORMAL_S=	${CC} ${AFLAGS} ${CPPFLAGS} -c $<
srt0.o: srt0.s
	${NORMAL_S}

# Explicit dependency for this.
boot.o: boot.c elfXX_exec.c

.include <bsd.prog.mk>
