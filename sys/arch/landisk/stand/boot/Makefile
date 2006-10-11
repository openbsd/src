#	$OpenBSD: Makefile,v 1.3 2006/10/11 17:19:06 drahn Exp $

PROG=	boot
SRCS=	srt0.S conf.c devs.c getsecs.c scifcons.c delay.c
LDFLAGS=-nostdlib -Ttext 0x8ff00000 -N -x -Bstatic -e start

INSTALL_STRIP=

S=	${.CURDIR}/../../../..
CPPFLAGS+=-D_STANDALONE
CPPFLAGS+=-DSH4
CPPFLAGS+=-nostdinc -I${.OBJDIR} -I${.CURDIR} -I${.CURDIR}/.. -I${S}
CPPFLAGS+=-DLOADADDRESS=0x8ff00000
CFLAGS+=-m4-nofpu

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

LIBGCC!=${CC} -print-libgcc-file-name
LDADD+= $(LIBGCC)

${PROG}: ${OBJS} ${DPADD}
	${LD} -o ${PROG}.sym ${LDFLAGS} \
		-Map ${PROG}.map -cref ${OBJS} ${LDADD}
	${OBJCOPY} -O binary ${PROG}.sym ${PROG}

NOMAN=

.include <bsd.prog.mk>
