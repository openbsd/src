#	$OpenBSD: Makefile,v 1.73 2018/08/29 15:56:46 robert Exp $

SUBDIR=ldconfig ldd
MAN=	ld.so.1

.include <bsd.own.mk>

.if defined(NOPIC)
NOPROG=
.else
PROG=	ld.so
BINMODE=444

VERSION_SCRIPT=	${.CURDIR}/Symbols.map
ELF_LDFLAGS+=--version-script=${VERSION_SCRIPT}

VPATH=${.CURDIR}/../../lib/libc/string

SRCS=	ldasm.S boot.c loader.c resolve.c dlfcn.c dl_printf.c rtld_machine.c
SRCS+=	path.c util.c sod.c strsep.c strtol.c dir.c library_subr.c
SRCS+=	dl_realpath.c dl_uname.c dl_dirname.c strlcat.c strlen.c trace.c
SRCS+=	malloc.c reallocarray.c tib.c ffs.c

syscall=__syscall close exit fstat __getcwd getdents getentropy getthrid \
	issetugid mprotect munmap open pledge read readlink sendsyslog \
	__set_tcb sysctl thrkill utrace write

GEN_PREFIX=\t.file "${@:R}.c"\n\#include "SYS.h"
.for _i in ${syscall}
OBJS+=dl_${_i}.o
dl_${_i}.o: SYS.h
	printf '${GEN_PREFIX}\nDL_SYSCALL(${_i})\n' | \
	    ${CC} ${DEBUG} -c -fPIC ${CFLAGS:M-[ID]*} ${CFLAGS:M-pipe} ${AINC} -P \
	    -x assembler-with-cpp - ${DFLAGS} -MF ${.TARGET:R}.d -o ${.TARGET}
.endfor

.if (${MACHINE_ARCH} == "i386")
SRCS+=	library_mquery.c
.else
SRCS+=	library.c
.endif

.include "${.CURDIR}/${MACHINE_CPU}/Makefile.inc"
.PATH:	${.CURDIR}/${MACHINE_CPU}

DEBUG?=	-g
CFLAGS += -fno-builtin
CFLAGS += -Wall -Werror
CFLAGS += -I${.CURDIR} -I${.CURDIR}/${MACHINE_CPU} \
	-D'DEF_WEAK(x)=asm("")' -D'DEF_STRONG(x)=asm("")' \
	-Dstrsep=_dl_strsep -Dstrlcat=_dl_strlcat -Dstrlen=_dl_strlen
CDIAGFLAGS=
INSTALL_STRIP=

ELF_LDFLAGS+=--shared -Bsymbolic --no-undefined

test_prog=	test-$(PROG)
CLEANFILES+=	test-$(PROG)
candidate=	$(PROG).test
CLEANFILES+=	${candidate}

$(test_prog):
	printf '#include <stdio.h>\nint main(int argc, char **argv){ printf("%%s: ", argv[0]); printf("%%s!\\n", argv[1] ? argv[1] : "foo"); }\n' | \
	$(CC) -P -x c - -Wl,-dynamic-linker,./$(candidate) -o $@

$(PROG): $(test_prog) ${VERSION_SCRIPT} $(OBJS)
.if defined(SYSPATCH_PATH)
	$(LD) ${DEBUG} -e _dl_start $(ELF_LDFLAGS) -o $(candidate) \
            `readelf -Ws ${SYSPATCH_PATH}/usr/libexec/${.TARGET} | \
            awk '/ FILE/{sub(".*/", "", $$NF); gsub(/\..*/, ".o", $$NF); print $$NF}' | \
            awk '!x[$$0]++'`
.else
	$(LD) ${DEBUG} -e _dl_start $(ELF_LDFLAGS) -o $(candidate) \
	    `echo ${OBJS} | tr " " "\n" | sort -R`
.endif
.ifndef CROSSDIR
	ulimit -c 0; [ "`${.OBJDIR}/$(test_prog) ok`" = "${.OBJDIR}/$(test_prog): ok!" ]
.endif
	cp $(candidate) $@
.endif

.include <bsd.prog.mk>

boot.o: boot.c
	${COMPILE.c} -fno-stack-protector ${.IMPSRC}

CLEANFILES+=	ld.so.a

all: ld.so.a

ld.so.a: ${OBJS} ${.CURDIR}/Symbols.map ${test_prog}
	ar cqD $@ $?

afterinstall: ld.so.a
	install -d -o root -g wheel -m 755 \
	    ${DESTDIR}/usr/share/relink/usr/libexec
	install -o ${BINOWN} -g ${BINGRP} -m ${NONBINMODE} \
	    ld.so.a ${DESTDIR}/usr/share/relink/usr/libexec/ld.so.a
