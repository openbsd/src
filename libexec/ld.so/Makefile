#	$OpenBSD: Makefile,v 1.19 2003/04/17 03:40:45 drahn Exp $

SUBDIR=ldconfig ldd
VPATH=${.CURDIR}/../../lib/libc/string:${.CURDIR}/../../sys/lib/libsa

NOMAN=
SRCS=	ldasm.S loader.c resolve.c dlfcn.c dl_printf.c rtld_machine.c
SRCS+=	util.c sod.c strsep.c strtol.c dir.c
.if (${MACHINE_ARCH} == "i386")
SRCS+=	library_mquery.c 
.else
SRCS+=	library.c 
.endif

PROG=	ld.so
MAN=	ld.so.8

.include "${.CURDIR}/${MACHINE_ARCH}/Makefile.inc"
.PATH:	${.CURDIR}/${MACHINE_ARCH}

CFLAGS += -Werror -Wall
CFLAGS += -I${.CURDIR} -I${.CURDIR}/${MACHINE_ARCH} \
	-Dstrsep=_dl_strsep -Dstrtol=_dl_strtol
INSTALL_STRIP=

ELF_LDFLAGS+=--shared -Bsymbolic

$(PROG):
	$(LD) -x -e _dl_start $(ELF_LDFLAGS) -o $(PROG) $(OBJS)

.include <bsd.prog.mk>
