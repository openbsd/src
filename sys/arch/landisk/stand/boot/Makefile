#	$OpenBSD: Makefile,v 1.6 2008/05/28 21:16:43 miod Exp $

MAN=	boot.8
MLINKS=	boot.8 boot.conf.5

.if ${MACHINE} == "landisk"
PROG=	boot
SRCS=	srt0.S conf.c devs.c getsecs.c scifcons.c delay.c
LDFLAGS=-nostdlib -Ttext 0x8ff00000 -N -x -Bstatic -e start
OBJCOPY?=objcopy

INSTALL_STRIP=

S=	${.CURDIR}/../../../..
CPPFLAGS+=-D_STANDALONE
CPPFLAGS+=-DSH4
CPPFLAGS+=-nostdinc -I${.OBJDIR} -I${.CURDIR} -I${.CURDIR}/.. -I${S}
CPPFLAGS+=-DLOADADDRESS=0x8ff00000
CFLAGS+=-m4-nofpu

.PATH:  ${S}/stand/boot
SRCS+=	boot.c cmd.c vars.c bootarg.c

.PATH:	${S}/lib/libkern/arch/sh ${S}/lib/libkern
SRCS+=	strlen.c strlcpy.c

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

LIBGCC!=${CC} -print-libgcc-file-name
LDADD+= $(LIBGCC)

${PROG}: ${OBJS} ${DPADD}
	${LD} -o ${PROG}.sym ${LDFLAGS} \
		-Map ${PROG}.map -cref ${OBJS} ${LDADD}
	${OBJCOPY} -O binary ${PROG}.sym ${PROG}

.else
NOPROG=
.endif

.include <bsd.prog.mk>
