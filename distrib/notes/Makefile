#	$OpenBSD: Makefile,v 1.11 2001/03/28 21:11:50 miod Exp $

NOPROG=
NOMAN=

M?=	$(MACHINE)
M_A?=	$(MACHINE_ARCH)

TARG=	INSTALL.$M
SRC=	$(.CURDIR)/INSTALL
DEP=	$(SRC) $(.CURDIR)/mirrors $(.CURDIR)/$M/whatis $(.CURDIR)/$M/contents \
	$(.CURDIR)/$M/hardware $(.CURDIR)/$M/xfer $(.CURDIR)/$M/prep \
	$(.CURDIR)/$M/install $(.CURDIR)/$M/upgrade $(.CURDIR)/donations \
	$(.CURDIR)/packages $(.CURDIR)/m4.common

all:	$(TARG)

allarchs:
	@cd $(.CURDIR); \
	    make MACHINE=alpha MACHINE_ARCH=alpha; \
	    make MACHINE=amiga MACHINE_ARCH=m68k; \
	    make MACHINE=hp300 MACHINE_ARCH=m68k; \
	    make MACHINE=i386 MACHINE_ARCH=i386; \
	    make MACHINE=mac68k MACHINE_ARCH=m68k; \
	    make MACHINE=mvme68k MACHINE_ARCH=m68k; \
	    make MACHINE=pmax MACHINE_ARCH=mips; \
	    make MACHINE=powerpc MACHINE_ARCH=powerpc; \
	    make MACHINE=sparc MACHINE_ARCH=sparc; \
	    make MACHINE=sun3 MACHINE_ARCH=m68k; \
	    make MACHINE=vax MACHINE_ARCH=vax ;

$(TARG): $(DEP)
	m4 -DOSREV=$(OSREV) -DOSrev=$(OSrev) -DINCLUDE=$(.CURDIR)/$M \
	-DMACHINE=$M -DMACHINE_ARCH=$(M_A) -Uunix $(SRC) > $@

clean cleandir:
	-/bin/rm -f INSTALL.*

beforeinstall:
	cp ${TARG} ${DESTDIR}/snapshot/

.include <bsd.prog.mk>
