#	$OpenBSD: Makefile,v 1.4 1996/11/13 21:23:13 niklas Exp $
#	$NetBSD: Makefile,v 1.6 1996/10/18 05:27:38 thorpej Exp $

.include <bsd.own.mk>			# for ELF_TOOLCHAIN definition

CFLAGS+=	-DLIBC_SCCS -DPIC -DDYNAMIC -DELFSIZE=64
CFLAGS+=	-I${.CURDIR}/../../../libexec/elf_rtld/include

.if !defined(ELF_TOOLCHAIN)
CFLAGS+=	-DECOFF_COMPAT
.endif

OBJS=		crt0.o gcrt0.o crtbegin.o crtend.o

CLEANFILES+=	core a.out

all: ${OBJS}

crt0.o: crt0.c
	@echo ${CC} ${CFLAGS} -DCRT0 -c ${.ALLSRC} -o ${.TARGET}
	@${CC} ${CFLAGS} -DCRT0 -c ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

gcrt0.o: crt0.c
	@echo ${CC} ${CFLAGS} -DMCRT0 -c ${.ALLSRC} -o ${.TARGET}
	@${CC} ${CFLAGS} -DMCRT0 -c ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtbegin.o: crtbegin.c
	@echo ${CC} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${CC} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtend.o: crtend.c
	@echo ${CC} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${CC} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

.include <bsd.prog.mk>
