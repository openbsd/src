#	$OpenBSD: Makefile,v 1.25 2013/01/18 04:10:16 jsing Exp $

MAN=	biosboot.8

.if ${MACHINE} == "i386"
S=	${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

PROG=	biosboot
SRCS=	biosboot.S
LD=ld
LDFLAGS=-nostdlib -Ttext 0 -N -x -Bstatic -nopie
INSTALL_STRIP=

${.CURDIR}/biosboot.S: assym.h

${PROG}: $(OBJS)
	@rm -f $(PROG)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS)
	@size $(PROG)

CPPFLAGS+=-DLOADADDR=$(LOADADDR) -DLINKADDR=$(LINKADDR) -DBOOTMAGIC=$(BOOTMAGIC)
CPPFLAGS+=${DEBUGFLAGS}
CFLAGS+=-fno-pie
#AFLAGS+=-Wa,-a
AFLAGS+=-fno-pie
.else
NOPROG=
.endif

.include <bsd.prog.mk>
