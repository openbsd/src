#	$OpenBSD: Makefile,v 1.16 1998/05/25 19:04:50 mickey Exp $

PROG=	biosboot
SRCS=	biosboot.S
LD=ld
LDFLAGS=-nostdlib -Ttext 0 -N -x -Bstatic
INSTALL_STRIP=
MAN=	biosboot.8
SADIR=	${.CURDIR}/..
CLEANFILES=	i386 machine

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
CPPFLAGS+=${DEBUGFLAGS} -DBEMBR
#AFLAGS+=-Wa,-a
