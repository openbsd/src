#	$NetBSD: Makefile.inc,v 1.5 1996/02/01 22:33:01 mycroft Exp $

.if !defined(__stand_makefile_inc)
__stand_makefile_inc=1

S= ${.CURDIR}/../../../${R}

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(kernlibdir)
.BEGIN:
	@([ -h machine ] || ln -s ${S}/arch/${MACHINE}/include machine)
.endif

RELOC_SUN4=	0x240000
RELOC_SUN4C=	0x340000
RELOC_SUN4M=	0x440000

# Note: a `RELOC' value of 0x340000 seems to work on most machines
RELOC?=		${RELOC_SUN4C}

INCLUDES+=	-I. -I${.CURDIR}/.. -I${S}/arch -I${S} -I${S}/lib/libsa
DEFS+=		-DSTANDALONE -DRELOC=${RELOC} -DSUN4 -DSUN4C -DSUN_BOOTPARAMS
CFLAGS+=	-O2 ${INCLUDES} ${DEFS}

srt0.o: srt0.S
	${CC} ${CFLAGS} -D_LOCORE -c ${.IMPSRC}

cleandir:
	rm -rf lib machine

.endif
