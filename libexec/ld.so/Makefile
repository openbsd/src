#	$OpenBSD: Makefile,v 1.15 2002/07/12 20:18:30 drahn Exp $

SUBDIR=ldconfig ldd
VPATH=${.CURDIR}/../../lib/libc/string:${.CURDIR}/../../sys/lib/libsa

NOMAN=
SRCS=	ldasm.S loader.c library.c resolve.c dlfcn.c dl_printf.c rtld_machine.c
SRCS+=	util.c
SRCS+=  sod.c strsep.c strtol.c
SRCS+=  dir.c
PROG=	ld.so
MAN=	ld.so.8

.if (${MACHINE_ARCH} == "sparc64")
CFLAGS += -fpic -msoft-float
AFLAGS += -fpic
.endif
.if (${MACHINE_ARCH} == "powerpc")
CFLAGS += -fpic -msoft-float
.endif
.if (${MACHINE_ARCH} == "alpha")
CFLAGS += -fpic -mno-fp-regs
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.include "${LIBCSRCDIR}/arch/alpha/Makefile.inc"
.endif

#CFLAGS += -Werror -Wall -Wno-uninitialized
CFLAGS += -Werror -Wall
CFLAGS += -I${.CURDIR} -DNO_UNDERSCORE -DVERBOSE_DLINKER \
	-D__PIC__ -I${.CURDIR}/${MACHINE_ARCH} \
	-Dstrsep=_dl_strsep -Dstrtol=_dl_strtol
INSTALL_STRIP=

.PATH:	${.CURDIR}/${MACHINE_ARCH}

ELF_LDFLAGS=--shared -Bsymbolic # using GNU ld
.if (${MACHINE_ARCH} == "powerpc")
ADDR=-Tdata 8000
ELF_LDFLAGS+=${ADDR} # using GNU ld
.endif

$(PROG):
	$(LD) -x -e _dl_start $(ELF_LDFLAGS) -o $(PROG) $(OBJS)

.include <bsd.prog.mk>
