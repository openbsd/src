#	$OpenBSD: Makefile,v 1.26 2017/01/21 00:46:32 guenther Exp $

OBJS=		crt0.o gcrt0.o
OBJS+=		crtbegin.o crtend.o
OBJS+=		crtbeginS.o crtendS.o
OBJS+=		rcrt0.o
SRCS=		crt0.c crtbegin.c crtbeginS.c crtend.c crtendS.c

CFLAGS+=	-I${.CURDIR}/${MACHINE_CPU}
CFLAGS+=	-I${.CURDIR}/../../libexec/ld.so
CFLAGS+=	-I${.CURDIR}/../../libexec/ld.so/${MACHINE_CPU}

# XXX "use -fno-omit-frame-pointer; the reason is almost crazy; pr#287"
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
CFLAGS+=	-fno-omit-frame-pointer
.endif

# Override powerpc default of -fPIE
# XXX if this is safe, why not override CFLAGS for alpha and sparc64 too?
# Does it work because the csu bits come first and get the first few GOT
# entries?
.if ${MACHINE_ARCH} == "powerpc"
CFLAGS+=	-fpie
.endif

.ifdef NOPIC
PICFLAG=
.elif ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "sparc64"
# The objects for shared libraries need to work for all possible
# libraries, so force big PIC where it matters.
# Arguably m88k should do this too, but maybe there aren't any
# libraries big enough to need it there?
PICFLAG=	-fPIC
.endif

RCFLAGS=-DRCRT0
# amd64 can access the stack protector before relocation has occurred.
# Other archs aren't so lucky
.if ${MACHINE_ARCH} != "amd64"
RCFLAGS+=-fno-stack-protector 
.endif

all: ${OBJS}

crt0.o: crt0.c
	@echo ${COMPILE.c} ${.CURDIR}/crt0.c -o ${.TARGET}
	@${COMPILE.c} ${.CURDIR}/crt0.c -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

gcrt0.o: crt0.c
	@echo ${COMPILE.c} -DMCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}
	@${COMPILE.c} -DMCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

rcrt0.o: crt0.c
	@echo ${COMPILE.c} ${RCFLAGS} ${.CURDIR}/crt0.c -o ${.TARGET}
	@${COMPILE.c} ${RCFLAGS} ${.CURDIR}/crt0.c -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtbegin.o: crtbegin.c
	@echo ${COMPILE.c} ${.CURDIR}/crtbegin.c -o ${.TARGET}
	@${COMPILE.c} ${.CURDIR}/crtbegin.c -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtbeginS.o: crtbeginS.c
	@echo ${COMPILE.c} ${PICFLAG} ${.CURDIR}/crtbeginS.c -o ${.TARGET}
	@${COMPILE.c} ${PICFLAG} ${.CURDIR}/crtbeginS.c -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtend.o: crtend.c
	@echo ${COMPILE.c} ${.CURDIR}/crtend.c -o ${.TARGET}
	@${COMPILE.c} ${.CURDIR}/crtend.c -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

crtendS.o: crtendS.c
	@echo ${COMPILE.c} ${PICFLAG} ${.CURDIR}/crtendS.c -o ${.TARGET}
	@${COMPILE.c} ${PICFLAG} ${.CURDIR}/crtendS.c -o ${.TARGET}.o
	@${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	@rm -f ${.TARGET}.o

realinstall:
	${INSTALL} ${INSTALL_COPY} -S -o ${BINOWN} -g ${BINGRP} -m 444 ${OBJS} \
	    ${DESTDIR}/usr/lib

afterdepend: .depend
	@sed -i 's/^\([^\.]*\).o[ ]*:/\1.o g\1.o r\1.o:/' .depend

.include <bsd.prog.mk>
