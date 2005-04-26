#	$OpenBSD: Makefile,v 1.4 2005/04/26 19:20:21 deraadt Exp $

MAN=	boot.8
MANSUBDIR=zaurus
MLINKS=	boot.8 boot.conf.8

.if ${MACHINE} == "zaurus"
PROG=	zboot
LDFLAGS+=-nostdlib -Bstatic
INSTALL_STRIP=

SRCS=	crt0.c

S=	${.CURDIR}/../../../..

CLEANFILES+= arm machine
.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(kernlibdir) && !make(obj)
.BEGIN:
	@([ X$(S) = X -o -h arm ] || ln -s $(S)/arch/arm/include arm)
	@([ X$(S) = X -o -h machine ] || ln -s $(S)/arch/zaurus/include machine)
.endif

# stand/boot
SRCS+=	boot.c cmd.c vars.c bootarg.c conf.c

# libsa
SRCS+=	close.c closeall.c cons.c ctime.c disklabel.c dkcksum.c fstat.c \
	lseek.c memcmp.c memcpy.c memset.c open.c printf.c read.c \
	readdir.c snprintf.c stat.c strerror.c strtol.c ufs.c

# libkern
SRCS+=	strcmp.c strlcpy.c strlen.c strncmp.c strncpy.c write.c \
	ashrdi3.c divsi3.S

# local overrides for Linux/zaurus
SRCS+=	alloc.c devopen.c exec.c exit.c loadfile.c machdep.c \
	unixdev.c unixsys.S

.PATH:	${S}/stand/boot
.PATH:	${S}/lib/libsa
.PATH:	${S}/lib/libkern
.PATH:	${S}/lib/libkern/arch/arm

.else
NOPROG=
.endif

.include <bsd.prog.mk>

AFLAGS+=-D_LOCORE
CPPFLAGS+=-D_STANDALONE
CPPFLAGS+=-I${S}/stand/boot -I${S}/lib/libsa -I. -I${.CURDIR} -I${S}
CFLAGS+=-fno-stack-protector -fno-builtin -fpack-struct
