#	$OpenBSD: Makefile,v 1.2 2007/11/26 08:36:49 deraadt Exp $

NOMAN=	no man
S	=${.CURDIR}/../../../..
CLEANFILES+=	cdboot.gdb cdboot.map

.if ${MACHINE} == "hppa64"

PROG=	cdboot

.PATH: ${.CURDIR}/../boot
SRCS+= srt0.S

SRCS=  srt0.S cdboot.c

.PATH: ${.CURDIR}/../libsa
SRCS+= pdc.c itecons.c dev_hppa64.c dk.c

.PATH: ${S}/lib/libkern/arch/hppa ${S}/lib/libkern
SRCS+= milli.S strlen.c strlcpy.c

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
DPADD+= $(SALIB)
LDADD+=	$(SALIB)

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
