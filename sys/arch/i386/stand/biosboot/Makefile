#	$OpenBSD: Makefile,v 1.19 2000/05/26 11:50:46 deraadt Exp $

MAN=	biosboot.8
MANSUBDIR=/i386

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

