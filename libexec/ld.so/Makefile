#	$OpenBSD: Makefile,v 1.37 2010/02/03 20:49:00 miod Exp $

SUBDIR=ldconfig ldd

MAN=	ld.so.1

.include <bsd.own.mk>
.if defined(NOPIC)
NOPROG=
.else
PROG=	ld.so
.endif

VPATH=${.CURDIR}/../../lib/libc/string

SRCS=	ldasm.S loader.c resolve.c dlfcn.c dl_printf.c rtld_machine.c
SRCS+=	util.c sod.c strsep.c strtol.c dir.c library_subr.c dl_prebind.c
.if (${MACHINE_ARCH} == "i386")
SRCS+=	library_mquery.c
.else
SRCS+=	library.c
.endif


.include "${.CURDIR}/${MACHINE_CPU}/Makefile.inc"
.PATH:	${.CURDIR}/${MACHINE_CPU}

CFLAGS += -Wall -Werror
CFLAGS += -I${.CURDIR} -I${.CURDIR}/${MACHINE_CPU} \
	-Dstrsep=_dl_strsep
INSTALL_STRIP=

ELF_LDFLAGS+=--shared -Bsymbolic --no-undefined

$(PROG):
	$(LD) -x -e _dl_start $(ELF_LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)

.include <bsd.prog.mk>
