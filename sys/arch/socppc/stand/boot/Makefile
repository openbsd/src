#	$OpenBSD: Makefile,v 1.10 2010/02/15 13:57:15 kettenis Exp $

.include "${.CURDIR}/../Makefile.inc"

MAN=	boot.8
MLINKS=	boot.8 boot.conf.5

.if ${MACHINE} == "socppc"
PROG=		boot
S=		${.CURDIR}/../../../..

.PATH:		${S}/stand/boot
.PATH:		${S}/lib/libsa

CPPFLAGS+=	-D_STANDALONE
CPPFLAGS+= 	-nostdinc -I../.. -I. -I${.CURDIR} -I${S}
CPPFLAGS+=	-DCONADDR=0xe0004500UL -DCONSPEED=115200
CPPFLAGS+=	-DNS16550_FREQ=266000000 -DMACHINE_CMD=cmd_machine

COPTS+=		-ffreestanding -fno-stack-protector -ffixed-r29 -msoft-float

SRCS=		start.S 
SRCS+=		boot.c cmd.c vars.c
SRCS+=		conf.c devopen.c exec.c machdep.c dev_socppc.c time.c
SRCS+=		clock.c ns16550.c wd.c wdc.c pciide.c wdc_obio.c fdt.c
SRCS+=		ctime.c strtol.c cmd_socppc.c

.PATH:	${S}/lib/libkern/arch/powerpc ${S}/lib/libkern
SRCS+=	strlcpy.c strlen.c ashrdi3.c

### find out what to use for libz
Z_AS=		library
.include "${S}/lib/libz/Makefile.inc"
LIBZ=		${ZLIB}

### find out what to use for libsa
SA_AS=          library
SAREL=
SA_ZLIB=	yes
USE_LOADFILE=	yes
.include "${S}/lib/libsa/Makefile.inc"
LIBSA=          ${SALIB}

STANDIR=	${.CURDIR}/..

RELOC=		0x100000
LDFLAGS=	-Ttext ${RELOC}
OBJCOPY?=	objcopy

${PROG}: ${OBJS} ${LIBSA} ${LIBZ}
	${LD} ${LDFLAGS} -o ${PROG}.elf ${OBJS} ${LIBSA} ${LIBZ}
	${OBJCOPY} -O binary ${PROG}.elf ${PROG}.bin
	${MKBOOT} -e ${RELOC} -l ${RELOC} ${PROG}.bin ${PROG}

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}/${PROG}
	${INSTALL} ${INSTALL_COPY} ${INSTALL_STRIP} -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${PROG}.elf ${DESTDIR}${BINDIR}/${PROG}.elf

.if !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../../${MACHINE}/include machine)
	@([ -h powerpc ] || ln -s ${.CURDIR}/../../../powerpc/include powerpc)
.NOPATH: machine powerpc
CLEANFILES+=	machine powerpc
.endif

.else
NOPROG=
.endif

.include <bsd.prog.mk>
