#	$OpenBSD: Makefile,v 1.1 2003/03/28 22:42:26 mickey Exp $

NOMAN=	no man
S	=${.CURDIR}/../../../..
CLEANFILES+=	cdboot.gdb cdboot.map

.if ${MACHINE} == "hppa"

PROG=	cdboot
SRCS=	srt0.S exec.c cdboot.c conf.c \
	machdep.c pdc.c itecons.c dev_hppa.c dk.c
LD?=	ld
LDFLAGS+=-Bstatic -nostartfiles -nostdlib -N -Ttext $(LINKADDR)
LDFLAGS+=-T ${.CURDIR}/ld.script -Map cdboot.map
SIZE?=	size
STRIP?=	strip
STANDIR=${.CURDIR}/..
CRTBEGIN=
CRTEND=

SAREL=
USE_LOADFILE=yes
.include "${S}/lib/libsa/Makefile.inc"
DPADD+= $(SALIB) $(LIBKERN)
LDADD+=	$(SALIB) $(LIBKERN)

.PATH:	${S}/stand/boot
.PATH:	${.CURDIR}/../boot
.PATH:	${.CURDIR}/../libsa

all:	${PROG}

clean::
	rm -f a.out [Ee]rrs mklog core *.core \
	    ${PROG} ${OBJS} ${LOBJS} ${CLEANFILES}

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${PROG} ${DESTDIR}${BINDIR}

# TODO check for <=32k
${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@cp ${PROG} ${PROG}.gdb
	${STRIP} ${PROG}
	@${SIZE} $(PROG)
	@${MKBOOT} -v ${PROG} ${PROG}.lif
	@mv ${PROG}.lif ${PROG}
.else
NOPROG=
.endif

.include <bsd.prog.mk>

CPPFLAGS+=-DRELOC=${LOADADDR} -DHEAP_LIMIT=${HEAP_LIMIT}
CFLAGS+=$(SACFLAGS) -I../.. -I${.CURDIR}/../libsa -I${S}/stand/boot
