#	$OpenBSD: Makefile,v 1.20 2009/10/27 23:59:58 deraadt Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/1/93

OBJS=		crt0.o gcrt0.o
OBJS+=		crtbegin.o crtend.o
OBJS+=		crtbeginS.o crtendS.o
SRCS=		crt0.c crtbegin.c crtbeginS.c crtend.c crtendS.c

ELFDIR=		${.CURDIR}/../common_elf
.PATH:		${ELFDIR}
CFLAGS+=	-I${ELFDIR} -I${.CURDIR}

all: ${OBJS}

crt0.o: crt0.c
	@echo ${COMPILE.c} -fpie -DCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}
	@${COMPILE.c} -fpie -DCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

gcrt0.o: crt0.c
	@echo ${COMPILE.c} -DMCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}
	@${COMPILE.c} -DMCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtbegin.o: crtbegin.c
	@echo ${COMPILE.c} -fpie ${ELFDIR}/crtbegin.c -o ${.TARGET}
	@${COMPILE.c} -fpie ${ELFDIR}/crtbegin.c -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtbeginS.o: crtbeginS.c
	@echo ${COMPILE.c} ${PICFLAG} ${ELFDIR}/crtbeginS.c -o ${.TARGET}
	@${COMPILE.c} ${PICFLAG} ${ELFDIR}/crtbeginS.c -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtend.o: crtend.c
	@echo ${COMPILE.c} -fpie ${ELFDIR}/crtend.c -o ${.TARGET}
	@${COMPILE.c} -fpie ${ELFDIR}/crtend.c -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtendS.o: crtendS.c
	@echo ${COMPILE.c} ${PICFLAG} ${ELFDIR}/crtendS.c -o ${.TARGET}
	@${COMPILE.c} ${PICFLAG} ${ELFDIR}/crtendS.c -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

afterdepend:: .depend
	@(TMP=/tmp/_depend$$$$; \
	    sed -e 's/^\([^\.]*\).o[ ]*:/\1.o g\1.o:/' \
	    < .depend > $$TMP; \
	mv $$TMP .depend)

.include <bsd.prog.mk>
