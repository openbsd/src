#	$OpenBSD: Makefile,v 1.40 2013/09/04 20:00:26 patrick Exp $

NOPROG=
NOMAN=

M?=	$(MACHINE)

.if exists(${M}/contents)

TARG=	INSTALL.$M
SRC=	$(.CURDIR)/INSTALL
DEP=	$(SRC) $(.CURDIR)/mirrors $(.CURDIR)/$M/whatis $(.CURDIR)/$M/contents \
	$(.CURDIR)/$M/hardware $(.CURDIR)/$M/xfer $(.CURDIR)/$M/prep \
	$(.CURDIR)/$M/install $(.CURDIR)/$M/upgrade ${.CURDIR}/$M/features \
	$(.CURDIR)/packages $(.CURDIR)/m4.common

all:	$(TARG)

allarchs:
	@cd $(.CURDIR); \
	for arch in alpha amd64 armish armv7 aviion hp300 hppa i386 landisk \
	    loongson luna88k macppc mvme68k mvme88k octeon sgi socppc \
	    sparc sparc64 vax zaurus; \
	do ${MAKE} M=$$arch; done

$(TARG): $(DEP)
	m4 -DOSREV=$(OSREV) -DOSrev=$(OSrev) -DINCLUDE=$(.CURDIR)/$M \
	-DMACHINE=$M -Uunix $(SRC) > $@

.else
all:
.endif

.ifdef RELEASEDIR
beforeinstall:
	cp ${TARG} ${RELEASEDIR}
.endif

clean:
	-/bin/rm -f INSTALL.*

.include <bsd.prog.mk>
