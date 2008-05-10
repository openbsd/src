#	$OpenBSD: Makefile,v 1.1 2008/05/10 20:06:26 kettenis Exp $

NOMAN=

.if ${MACHINE} == "socppc"
PROG=		boot
S=		${.CURDIR}/../../../..

.PATH:		${S}/stand/boot
.PATH:		${S}/lib/libsa

CPPFLAGS+=	-D_STANDALONE
CPPFLAGS+= 	-nostdinc -I../.. -I. -I${.CURDIR} -I${S}
CPPFLAGS+=	-DCONADDR=0xe0004500UL -DCONSPEED=115200
CPPFLAGS+=	-DNS16550_FREQ=266000000

COPTS+=		-ffreestanding -fno-stack-protector -ffixed-r29

SRCS=		start.S 
SRCS+=		boot.c cmd.c vars.c
SRCS+=		conf.c devopen.c exec.c machdep.c dev_socppc.c time.c
SRCS+=		clock.c ns16550.c wd.c wdc.c pciide.c
SRCS+=		ctime.c strtol.c

.PATH:	${S}/lib/libkern/arch/powerpc ${S}/lib/libkern
#SRCS+=	divsi3.S divdi3.c moddi3.c qdivrem.c strlcpy.c strlen.c ashrdi3.c
SRCS+=	strlcpy.c strlen.c ashrdi3.c

### find out what to use for libsa
SA_AS=          library
SAREL=
USE_LOADFILE=	yes
.include "${S}/lib/libsa/Makefile.inc"
LIBSA=          ${SALIB}

LDFLAGS=	-Ttext 0x100000

${PROG}: ${OBJS} ${LIBSA}
	${LD} ${LDFLAGS} -o boot ${OBJS} ${LIBSA} ${LIBSA}

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
