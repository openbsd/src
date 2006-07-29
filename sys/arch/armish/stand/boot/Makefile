#	$OpenBSD: Makefile,v 1.4 2006/07/29 15:01:49 kettenis Exp $

NOMAN=

.if ${MACHINE} == "armish"
PROG=		boot
S=		${.CURDIR}/../../../..

.PATH:		${S}/stand/boot
.PATH:		${S}/lib/libsa

CPPFLAGS+=	-D_STANDALONE
CPPFLAGS+= 	-nostdinc -I../.. -I. -I${.CURDIR} -I${S}
CPPFLAGS+=	-DCONADDR=0xfe800000UL -DCONSPEED=115200

COPTS+=		-ffreestanding -fno-stack-protector

SRCS=		start.S 
SRCS+=		boot.c cmd.c vars.c
SRCS+=		conf.c devopen.c exec.c machdep.c dev_armish.c time.c
SRCS+=		clock.c ns16550.c wd.c wdc.c pciide.c
SRCS+=		ctime.c strtol.c

### find out what to use for libkern
KERN_AS=        library
.include "${S}/lib/libkern/Makefile.inc"
LIBKERN=        ${KERNLIB}

### find out what to use for libsa
SA_AS=          library
SAREL=
USE_LOADFILE=	yes
.include "${S}/lib/libsa/Makefile.inc"
LIBSA=          ${SALIB}

LDFLAGS=	-T ${.CURDIR}/ldscript

${PROG}: ${OBJS} ${LIBKERN} ${LIBSA}
	${LD} ${LDFLAGS} -o boot ${OBJS} ${LIBSA} ${LIBKERN} ${LIBSA}

.if !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../../${MACHINE}/include machine)
	@([ -h arm ] || ln -s ${.CURDIR}/../../../arm/include arm)
.NOPATH: machine arm
CLEANFILES+=	machine arm
.endif

.else
NOPROG=
.endif

.include <bsd.prog.mk>
