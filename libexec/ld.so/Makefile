#	$OpenBSD: Makefile,v 1.22 2003/07/06 20:03:57 deraadt Exp $

SUBDIR=ldconfig ldd
VPATH=${.CURDIR}/../../lib/libc/string

SRCS=	ldasm.S loader.c resolve.c dlfcn.c dl_printf.c rtld_machine.c
SRCS+=	util.c sod.c strsep.c strtol.c dir.c
.if (${MACHINE_ARCH} == "i386")
SRCS+=	library_mquery.c
.else
SRCS+=	library.c
.endif

PROG=	ld.so
MAN=	ld.so.1

.include "${.CURDIR}/${MACHINE_ARCH}/Makefile.inc"
.PATH:	${.CURDIR}/${MACHINE_ARCH}

CFLAGS += -Werror -Wall
CFLAGS += -I${.CURDIR} -I${.CURDIR}/${MACHINE_ARCH} \
	-Dstrsep=_dl_strsep
INSTALL_STRIP=

ELF_LDFLAGS+=--shared -Bsymbolic

$(PROG):
	$(LD) -x -e _dl_start $(ELF_LDFLAGS) -o $(PROG) $(OBJS)

.include <bsd.prog.mk>
