#	$OpenBSD: Makefile,v 1.5 1999/08/20 14:11:34 niklas Exp $
#	from: @(#)Makefile	5.5 (Berkeley) 5/21/91

CFLAGS+=	-fno-omit-frame-pointer -DLIBC_SCCS -I${.CURDIR}/..
OBJS=		crt0.o gcrt0.o scrt0.o
SRCS=		crt0.c

all: ${OBJS}

crt0.o: ${SRCS}
	@echo "${COMPILE.c} -DCRT0 -DDYNAMIC ${.CURDIR}/${SRCS} -o ${.TARGET}"
	@${COMPILE.c} -DCRT0 -DDYNAMIC ${.CURDIR}/${SRCS} -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

gcrt0.o: ${SRCS}
	@echo "${COMPILE.c} -DMCRT0 ${.CURDIR}/${SRCS} -o ${.TARGET}"
	@${COMPILE.c} -DMCRT0 ${.CURDIR}/${SRCS} -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

scrt0.o: ${SRCS}
	@echo "${COMPILE.c} -DSCRT0 ${.CURDIR}/${SRCS} -o ${.TARGET}"
	@${COMPILE.c} -DSCRT0 ${.CURDIR}/${SRCS} -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

.if make(depend)
CPPFLAGS+= -DDYNAMIC
.endif

afterdepend: .depend
	@(TMP=/tmp/_depend$$$$; \
		sed -e 's/^\([^\.]*\).o[ ]*:/\1.o g\1.o s\1.o:/' \
		< .depend > $$TMP; \
	mv $$TMP .depend)

.include <bsd.prog.mk>
