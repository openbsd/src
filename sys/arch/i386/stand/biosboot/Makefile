#	$OpenBSD: Makefile,v 1.20 2000/06/08 00:56:07 mickey Exp $

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

CPPFLAGS+=-DLOADADDR=$(LOADADDR) -DBOOTMAGIC=$(BOOTMAGIC)
CPPFLAGS+=${DEBUGFLAGS}
#AFLAGS+=-Wa,-a
.else
NOPROG=
.endif

.include <bsd.prog.mk>

