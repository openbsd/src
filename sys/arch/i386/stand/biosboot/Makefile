#	$OpenBSD: Makefile,v 1.22 2012/08/21 14:46:18 pascal Exp $

MAN=	biosboot.8

.if ${MACHINE} == "i386"
PROG=	biosboot
SRCS=	biosboot.S
LD=ld
LDFLAGS=-nostdlib -Ttext 0 -N -x -Bstatic
INSTALL_STRIP=
SADIR=	${.CURDIR}/..

${PROG}: $(OBJS) $(DPADD)
	@rm -f $(PROG)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@size $(PROG)

CPPFLAGS+=-DLOADADDR=$(LOADADDR) -DLINKADDR=$(LINKADDR) -DBOOTMAGIC=$(BOOTMAGIC)
CPPFLAGS+=${DEBUGFLAGS}
CFLAGS+=${NOPIE_FLAGS}
#AFLAGS+=-Wa,-a
AFLAGS+=${NOPIE_FLAGS}
.else
NOPROG=
.endif

.include <bsd.prog.mk>

