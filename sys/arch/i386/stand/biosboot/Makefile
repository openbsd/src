#	$OpenBSD: Makefile,v 1.3 1997/03/31 23:06:19 mickey Exp $

PROG=	biosboot
SRCS=	biosboot.S
# AFLAGS+=-Wa,-a
CPPFLAGS+=-DSTART=$(START) -DBOOTMAGIC=$(BOOTMAGIC) -DBOOTREL=$(BOOTREL)
LDFLAGS=-Wl,-T0,-N,-x -nostdlib
STRIP=
MAN=	biosboot.8
LINKS=	${BINDIR}/biosboot ${BINDIR}/fdboot \
	${BINDIR}/biosboot ${BINDIR}/wdboot \
	${BINDIR}/biosboot ${BINDIR}/sdboot \

all:	machine-links

machine-links:
	@rm -f i386 machine
	@ln -fs ${.CURDIR}/../.. i386
	@ln -fs ${.CURDIR}/../../include machine

.include <bsd.prog.mk>

