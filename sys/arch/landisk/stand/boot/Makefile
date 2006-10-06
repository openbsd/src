#	$OpenBSD: Makefile,v 1.1 2006/10/06 21:48:50 mickey Exp $

PROG=	boot
SRCS=	srt0.S conf.c devs.c getsecs.c scifcons.c delay.c
LDFLAGS=-nostdlib -Ttext 0x8ff00000 -N -x -Bstatic -e start

INSTALL_STRIP=

S=	${.CURDIR}/../../../..
CPPFLAGS+=-D_STANDALONE
CPPFLAGS+=-DSH4
CPPFLAGS+=-nostdinc -I${.OBJDIR} -I${.CURDIR} -I${.CURDIR}/.. -I${S}
CPPFLAGS+=-DLOADADDRESS=0x8ff00000

.PATH:  ${S}/stand/boot
SRCS+=	boot.c cmd.c vars.c bootarg.c

SAREL=
SA_ZLIB=
USE_LOADFILE=
.PATH:  ${S}/lib/libsa
SRCS+=	ctime.c strtol.c
.include "${S}/lib/libsa/Makefile.inc"
DPADD+= $(SALIB)
LDADD+= $(SALIB)

KERN_AS?=	library
.include "${S}/lib/libkern/Makefile.inc"
DPADD+= $(KERNLIB)
LDADD+= $(KERNLIB)

Z_AS?=	library
.include "${S}/lib/libz/Makefile.inc"
DPADD+= $(ZLIB)
LDADD+= $(ZLIB)

${PROG}: ${OBJS} ${DPADD}
	${LD} -o ${PROG}.sym ${LDFLAGS} \
		-Map ${PROG}.map -cref ${OBJS} ${LDADD}
	${OBJCOPY} -O binary ${PROG}.sym ${PROG}

.include <bsd.prog.mk>
