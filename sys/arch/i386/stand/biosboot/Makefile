#	$OpenBSD: Makefile,v 1.12 1998/03/30 07:00:28 deraadt Exp $

PROG=	biosboot
SRCS=	biosboot.S
LD=ld
LDFLAGS=-nostdlib -Ttext 0 -N -x -Bstatic
INSTALL_STRIP=
MAN=	biosboot.8
SADIR=	${.CURDIR}/..

all:	machine-links

machine-links:
	@rm -f i386 machine
	@ln -fs ${.CURDIR}/../.. i386
	@ln -fs ${.CURDIR}/../../include machine

${PROG}: $(OBJS) $(DPADD)
	@rm -f $(PROG)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@size $(PROG)

.include <bsd.prog.mk>

CPPFLAGS+=-DLOADADDR=$(LOADADDR) -DBOOTMAGIC=$(BOOTMAGIC) -DBOOTREL=$(BOOTREL)
CPPFLAGS+=${DEBUGFLAGS} -DBEMBR -DDEBUG
#AFLAGS+=-Wa,-a
