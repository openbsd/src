#	$OpenBSDe Makefile,v 1.6 2000/06/28 04:56:44 rahnds Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/1/93

CFLAGS+=	-DLIBC_SCCS
OBJS=		crt0.o gcrt0.o scrt0.o crtbegin.o crtend.o crtbeginS.o crtendS.o 
PICFLAG?=-fpic

CLEANFILES+=	core a.out
.PATH: ${.CURDIR}/../common_elf

all: ${OBJS}

crt0.o: crt0.c
	${CC} ${CFLAGS} -c -DCRT0 ${.ALLSRC}  -o $@
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

gcrt0.o: crt0.c
	${CC} ${CFLAGS} -c -DMCRT0 ${.ALLSRC}  -o $@
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

scrt0.o: crt0.c
	${CC} ${CFLAGS} -c -DSRT0 ${.ALLSRC}  -o $@
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

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

crtbeginS.o: crtbeginS.c
	@echo ${CC} ${PICFLAG} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${CC} ${PICFLAG} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o
 
crtendS.o: crtendS.c
	@echo ${CC} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}
	@${CC} ${CFLAGS} -c ${.ALLSRC} -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
		${DESTDIR}/usr/lib

depend lint tags:

.include <bsd.prog.mk>
