#	$OpenBSD: Makefile,v 1.2 2000/09/08 04:23:45 rahnds Exp $

SUBDIR=libdl ldconfig
#CFLAGS =
.if (${MACHINE_ARCH} == "powerpc")
CFLAGS += -fpic -msoft-float
.endif
CFLAGS += -I${.CURDIR} -DNO_UNDERSCORE -DVERBOSE_DLINKER \
	-DUSE_CACHE -D__PIC__ -I${.CURDIR}/${MACHINE_ARCH}
#CFLAGS += -g
VPATH=${.CURDIR}/../../lib/libc/string:${.CURDIR}/../../sys/lib/libsa

NOMAN=
SRCS=	ldasm.S loader.c library.c resolve.c dlfcn.c dl_printf.c rtld_machine.c
SRCS+=  sod.c strcmp.c strlen.c strncmp.c strsep.c strtol.c strdup.c strcpy.c
OBJS+=  strchr.o
PROG=	ld.so
MAN=	ld.so.8
BINDIR=/usr/libexec
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
.include <bsd.subdir.mk>
