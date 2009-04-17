#	$OpenBSD: Makefile,v 1.32 2009/04/17 03:58:54 deraadt Exp $

NOPROG=
NOMAN=

M?=	$(MACHINE)

.if exists(${M}/contents)

TARG=	INSTALL.$M
SRC=	$(.CURDIR)/INSTALL
DEP=	$(SRC) $(.CURDIR)/mirrors $(.CURDIR)/$M/whatis $(.CURDIR)/$M/contents \
	$(.CURDIR)/$M/hardware $(.CURDIR)/$M/xfer $(.CURDIR)/$M/prep \
	$(.CURDIR)/$M/install $(.CURDIR)/$M/upgrade \
	$(.CURDIR)/packages $(.CURDIR)/m4.common

all:	$(TARG)

allarchs:
	@cd $(.CURDIR); \
	for arch in alpha amd64 armish aviion hp300 hppa i386 landisk \
	    luna88k mac68k macppc mvme68k mvme88k sgi socppc sparc sparc64 \
	    vax zaurus; \
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
