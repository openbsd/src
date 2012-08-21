#	$OpenBSD: Makefile,v 1.4 2012/08/21 14:46:18 pascal Exp $

MAN=	biosboot.8

.if ${MACHINE} == "amd64"
PROG=	biosboot
SRCS=	biosboot.S
LD=ld
LDFLAGS=-melf_i386 -nostdlib -Ttext 0 -N -x -Bstatic
LDFLAGS+=-L/usr/libdata 
INSTALL_STRIP=
SADIR=	${.CURDIR}/..

${PROG}: $(OBJS) $(DPADD)
	@rm -f $(PROG)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@size $(PROG)

CPPFLAGS+=-DLOADADDR=$(LOADADDR) -DLINKADDR=$(LINKADDR) -DBOOTMAGIC=$(BOOTMAGIC)
CPPFLAGS+=${DEBUGFLAGS}
CPPFLAGS+=${NOPIE_FLAGS}
AFLAGS+=-m32 # -Wa,-a
AFLAGS+=${NOPIE_FLAGS}
.else
NOPROG=
.endif

.include <bsd.prog.mk>

