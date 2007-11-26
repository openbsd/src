#	$OpenBSD: Makefile,v 1.9 2007/11/26 10:26:03 deraadt Exp $

MAN=	boot.8
MANSUBDIR=zaurus
MLINKS=	boot.8 boot.conf.5

.if ${MACHINE} == "zaurus"
PROG=	zboot
LDFLAGS+=-nostdlib -Bstatic
INSTALL_STRIP=

SRCS=	crt0.c

S=	${.CURDIR}/../../../..

CLEANFILES+= arm machine
.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(obj)
.BEGIN:
	@([ X$(S) = X -o -h arm ] || ln -s $(S)/arch/arm/include arm)
	@([ X$(S) = X -o -h machine ] || ln -s $(S)/arch/zaurus/include machine)
.endif

# stand/boot
SRCS+=	boot.c vars.c bootarg.c conf.c

# libsa
SRCS+=	close.c closeall.c cons.c ctime.c disklabel.c dkcksum.c fstat.c \
	lseek.c memcmp.c memcpy.c memset.c open.c printf.c read.c \
	readdir.c snprintf.c stat.c strerror.c strtol.c ufs.c

.PATH:	${S}/lib/libkern/arch/arm ${S}/lib/libkern
SRCS+=	strcmp.c strlcpy.c strlen.c strncmp.c strncpy.c write.c \
	ashrdi3.c divsi3.S

# local overrides and additions
SRCS+=	alloc.c cmd.c devopen.c diskprobe.c exec.c exit.c loadfile.c \
	machdep.c termios.c unixcons.c unixdev.c unixsys.S

.PATH:	${S}/stand/boot
.PATH:	${S}/lib/libsa

.else
NOPROG=
.endif

.include <bsd.prog.mk>

AFLAGS+=-D_LOCORE
CPPFLAGS+=-D_STANDALONE
CPPFLAGS+=-I${S}/stand/boot -I${S}/lib/libsa -I. -I${.CURDIR} -I${S}
CFLAGS+=-fno-stack-protector -fno-builtin -fpack-struct
