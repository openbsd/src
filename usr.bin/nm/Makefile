#	$OpenBSD: Makefile,v 1.6 2004/10/09 20:26:57 mickey Exp $

TARGET_MACHINE_ARCH?=	${MACHINE_ARCH}

.if ${TARGET_MACHINE_ARCH} != ${MACHINE_ARCH}
# XXX should make this automatic
.if ${TARGET_MACHINE_ARCH} == "i386"
CFLAGS+=	-DMID_MACHINE_OVERRIDE=MID_I386
.elif ${TARGET_MACHINE_ARCH} == "m68k"
CFLAGS+=	-DMID_MACHINE_OVERRIDE=MID_M68K
.elif ${TARGET_MACHINE_ARCH} == "ns32k"
CFLAGS+=	-DMID_MACHINE_OVERRIDE=MID_NS32K
.elif ${TARGET_MACHINE_ARCH} == "sparc"
CFLAGS+=	-DMID_MACHINE_OVERRIDE=MID_SPARC
.elif ${TARGET_MACHINE_ARCH} == "vax"
CFLAGS+=	-DMID_MACHINE_OVERRIDE=MID_VAX
.endif
.endif

PROG=	nm
SRCS=	nm.c elf32.c elf64.c
CLEANFILES+=elf32.c elf64.c
CPPFLAGS+=-I${.CURDIR}
LINKS=	${BINDIR}/nm ${BINDIR}/size
MAN=	nm.1 size.1

elf32.c: ${.CURDIR}/elf.c
	echo '#define ELFSIZE 32' | cat - $> > ${.TARGET}

elf64.c: ${.CURDIR}/elf.c
	echo '#define ELFSIZE 64' | cat - $> > ${.TARGET}

.include <bsd.prog.mk>
