#	$OpenBSD: Makefile,v 1.7 2001/01/25 05:37:20 art Exp $
#	$NetBSD: Makefile,v 1.6 1996/10/18 05:27:38 thorpej Exp $

.include <bsd.own.mk>			# for ELF_TOOLCHAIN definition

CFLAGS+=	-DLIBC_SCCS -DPIC -DELFSIZE=64
CFLAGS+=	-I${.CURDIR}/../../../libexec/elf_rtld/include

.if !defined(ELF_TOOLCHAIN)
CFLAGS+=	-DECOFF_COMPAT
.endif

OBJS=		crt0.o gcrt0.o crtbegin.o crtend.o crtbeginS.o crtendS.o

CLEANFILES+=	core a.out

PICFLAG?=-fpic

all: ${OBJS}

crt0.o: crt0.c
	${COMPILE.c} -DCRT0 ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

gcrt0.o: crt0.c
	${COMPILE.c} -DMCRT0 ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtbegin.o: crtbegin.c
	${COMPILE.c} ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtbeginS.o: crtbegin.c
	${COMPILE.c} ${PICFLAG} ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtend.o: crtend.c
	${COMPILE.c} ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtendS.o: crtend.c
	${COMPILE.c} ${PICFLAG} ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

.include <bsd.prog.mk>
