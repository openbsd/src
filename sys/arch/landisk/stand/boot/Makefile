#	$OpenBSD: Makefile,v 1.13 2016/07/30 03:25:48 guenther Exp $

MAN=	boot.8

.if ${MACHINE} == "landisk"
PROG=	boot
SRCS=	srt0.S conf.c devs.c getsecs.c scifcons.c delay.c
LDFLAGS=-nostdlib -Ttext 0x8ff00000 -N -x -Bstatic -e start -nopie -znorelro
OBJCOPY?=objcopy

INSTALL_STRIP=

S=	${.CURDIR}/../../../..
CPPFLAGS+=-D_STANDALONE
CPPFLAGS+=-DSH4
CPPFLAGS+=-nostdinc -I${.OBJDIR} -I${.CURDIR} -I${.CURDIR}/.. -I${S}
CPPFLAGS+=-DLOADADDRESS=0x8ff00000
CFLAGS+=-m4-nofpu -fno-pie
AFLAGS+= -fno-pie

.PATH:  ${S}/stand/boot
SRCS+=	boot.c cmd.c vars.c bootarg.c

.PATH:	${S}/lib/libkern/arch/sh ${S}/lib/libkern
SRCS+=	getchar.c putchar.c strlen.c strlcpy.c
SRCS+=	ashrdi3.c moddi3.c sdivsi3.S udivsi3.S

SAREL=
SA_ZLIB=
USE_LOADFILE=
.PATH:  ${S}/lib/libsa
SRCS+=	ctime.c strtol.c
.include "${S}/lib/libsa/Makefile.inc"
DPADD+= $(SALIB)
LDADD+= $(SALIB)

Z_AS?=	library
.include "${S}/lib/libz/Makefile.inc"
DPADD+= $(ZLIB)
LDADD+= $(ZLIB)

${PROG}: ${OBJS} ${DPADD}
	${LD} -o ${PROG}.sym ${LDFLAGS} \
		-Map ${PROG}.map -cref ${OBJS} ${LDADD}
	${OBJCOPY} -O binary ${PROG}.sym ${PROG}

.else
NOPROG=
.endif

.include <bsd.prog.mk>
