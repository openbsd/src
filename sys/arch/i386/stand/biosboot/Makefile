#	$OpenBSD: Makefile,v 1.4 1997/04/14 11:06:46 deraadt Exp $

PROG=	biosboot
SRCS=	biosboot.S
# AFLAGS+=-Wa,-a
CPPFLAGS+=-DSTART=$(START) -DBOOTMAGIC=$(BOOTMAGIC) -DBOOTREL=$(BOOTREL)
LDFLAGS=-Wl,-T0,-N,-x -nostdlib
STRIP=
MAN=	biosboot.8

all:	machine-links

machine-links:
	@rm -f i386 machine
	@ln -fs ${.CURDIR}/../.. i386
	@ln -fs ${.CURDIR}/../../include machine

.include <bsd.prog.mk>

