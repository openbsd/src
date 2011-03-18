#	$OpenBSD: Makefile.inc,v 1.17 2011/03/18 06:21:40 matthew Exp $
#	$NetBSD: Makefile.inc,v 1.5 1996/02/01 22:33:01 mycroft Exp $

BINDIR=	/usr/mdec

# sun4:
#	The first 8MB of physical memory are mapped 1:1.
# sun4c:
#	Worst case sun4c systems have their memory in non-contiguous 4MB
#	areas, therefore we must ensure the boot code never crosses a 4MB
#	boundary.
#	A SPARCstation IPC with 12MB memory in 1MB SIMMs, running PROM 2.9,
#	only maps virtual memory up to 0x3aefff.
# sun4m:
#	The minimal contiguous memory area will be 8MB (SS4/SS5 8MB DIMMs),
#	but there is no guarantee anything will be mapped at address zero
#	if the DIMM are set in a pathological (dis)order, as in:
#		ok cd /memory
#		ok .attributes
#		reg                      00000000  08000000  00800000 
#		                         00000000  0a000000  00800000 
#		                         00000000  0c000000  00800000 
#		                         00000000  0e000000  00800000 
#		available                00000000  0e000000  007a6000 
#		                         00000000  0c000000  00800000 
#		                         00000000  0a000000  00800000 
#		                         00000000  08000000  00800000 
#		name                     memory
#	where the four 8MB DIMMs are mapped at 32MB intervals from 128MB
#	onwards.
# sun4d:
#	Similar to sun4m, but the minimal contiguous memory area will be 32MB,
#	and all physical memory will be packed contiguous from address zero
#	onwards.
#
# The following values of RELOC and RELOC2 have been choosen to work on as
# many systems as possible, yet allowing small enough kernels to be loaded
# at the beginning of the physical memory.
RELOC=		0x358000
RELOC2=		0x398000

CFLAGS+=	-fno-stack-protector

# XXX <machine/param.h> incorrectly uses STANDALONE instead of _STANDALONE
DEFS=		-DSTANDALONE -D_STANDALONE -DRELOC=${RELOC} -DSUN_BOOTPARAMS \
		-DSUN4 -DSUN4C -DSUN4D -DSUN4M

### Figure out what to use for libsa
LIBSADIR=		${.CURDIR}/../libsa

.if exists(${LIBSADIR}/${__objdir})
LIBSAOBJDIR=		${LIBSADIR}/${__objdir}
.else
LIBSAOBJDIR=		${LIBSADIR}
.endif

LIBSA=			${LIBSAOBJDIR}/libsa.a

### Figure out what to use for libz
LIBZDIR=		${.CURDIR}/../libz

.if exists(${LIBZDIR}/${__objdir})
LIBZOBJDIR=		${LIBZDIR}/${__objdir}
.else
LIBZOBJDIR=		${LIBZDIR}
.endif

LIBZ=			${LIBZOBJDIR}/libz.a
