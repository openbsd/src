#	$OpenBSD: Makefile,v 1.3 2006/07/29 00:48:52 deraadt Exp $

NOMAN=

.if ${MACHINE} == "armish"
PROG=		boot
S=		${.CURDIR}/../../../..

CPPFLAGS+= 	-nostdinc -I../.. -I. -I${S} -D_STANDALONE
CPPFLAGS+=	-DCONSPEED=115200
CPPFLAGS+=	-DCONADDR=0xfe800000UL

COPTS+=		-ffreestanding -fno-stack-protector -malignment-traps

SRCS=		start.S boot.c conf.c clock.c ns16550.c wd.c wdc.c pciide.c
SRCS+=		devopen.c exec.c

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
