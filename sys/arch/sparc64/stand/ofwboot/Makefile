#	$OpenBSD: Makefile,v 1.5 2001/08/22 10:41:46 art Exp $
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
CFLAGS+=	${COPTS} ${CEXTRAFLAGS}
CPPFLAGS+=	-D_STANDALONE -DSUN4U -nostdinc
#CPPFLAGS+=	-DNETIF_DEBUG 

MKMAN=		no
STRIPFLAG=
BINMODE=	444
OBJCOPY?=	objcopy

NEWVERSWHAT=	"OpenFirmware Boot"

#
# Elf64 defaults to 1MB
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
CPPFLAGS+=	-DSPARC_BOOT_AOUT
CPPFLAGS+=	-DSPARC_BOOT_ELF
CPPFLAGS+=	-DSPARC_BOOT_UFS
CPPFLAGS+=	-DSPARC_BOOT_NFS
#CPPFLAGS+=	-DSPARC_BOOT_HSFS

.NOPATH: machine sparc64
beforedepend: machine sparc64

machine:
	rm -f machine
	ln -s sparc64 machine

sparc64:
	rm -f sparc64
	ln -s ${.CURDIR}/../../../sparc64/include sparc64

vers.c: version
	sh ${S}/conf/newvers.sh ${CURDIR}/version "sparc64" ${NEWVERSWHAT}

${PROG}: ${OBJS} ${LIBSA} ${LIBZ} ${LIBKERN}
	${LD} -N -p -Ttext ${RELOC} -e ${ENTRY} >lderr -o ${PROG} \
	    ${OBJS} -L${LIBSADIR} ${LIBSA} -L${LIBKERNDIR} ${LIBKERN} \
	    -L${LIBZDIR} ${LIBZ}

.include <bsd.prog.mk>

.if ${MACHINE_ARCH} == "sparc64"
AFLAGS+=		-Wa,-Av9a
CEXTRAFLAGS?=		-D_LP64
.else
AFLAGS+=		-Wa,-Av8plusa
.endif

.if CROSS
AFLAGS+=		-x assembler-with-cpp -traditional-cpp -D_LOCORE -D__ELF__
CEXTRAFLAGS?=		-D_LP64
.else
AFLAGS+=		-x assembler-with-cpp -traditional-cpp -D_LOCORE -D__ELF__ ${CEXTRAFLAGS}
.endif

NORMAL_S=	${CC} ${AFLAGS} ${CPPFLAGS} -c $<
srt0.o: srt0.s
	${NORMAL_S}

# Explicit dependency for this.
boot.o: boot.c elfXX_exec.c
