#	$OpenBSD: Makefile,v 1.10 2001/09/24 23:38:09 art Exp $

SUBDIR=libdl ldconfig ldd
VPATH=${.CURDIR}/../../lib/libc/string:${.CURDIR}/../../sys/lib/libsa

NOMAN=
SRCS=	ldasm.S loader.c library.c resolve.c dlfcn.c dl_printf.c rtld_machine.c
SRCS+=  sod.c strcmp.c strncmp.c strsep.c strtol.c strdup.c strcpy.c
OBJS+=  strchr.o
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
CFLAGS += -Werror
CFLAGS += -fpic -mno-fp-regs
LIBCSRCDIR=${.CURDIR}/../../lib/libc
.include "${LIBCSRCDIR}/arch/alpha/Makefile.inc"
.endif
CFLAGS += -I${.CURDIR} -DNO_UNDERSCORE -DVERBOSE_DLINKER \
	-DUSE_CACHE -D__PIC__ -I${.CURDIR}/${MACHINE_ARCH}
#CFLAGS += -g -DDL_PRINTF_DEBUG
INSTALL_STRIP=

.PATH:	${.CURDIR}/${MACHINE_ARCH}

.if (${MACHINE_ARCH} == "powerpc")
#ADDR=-Ttext 21860000 -Tdata 21863000
ADDR=-Tdata 8000
ELF_LDFLAGS=--shared -Bsymbolic ${ADDR} # using GNU ld
.else
ELF_LDFLAGS=--shared -Bsymbolic # using GNU ld
.endif

$(PROG):
	$(LD) -x -e _dl_start $(ELF_LDFLAGS) -o $(PROG) $(OBJS)

.include <bsd.prog.mk>
