#	$OpenBSD: Makefile,v 1.14 2001/09/01 16:50:19 drahn Exp $

NOPROG=
NOMAN=

M?=	$(MACHINE)
M_A?=	$(MACHINE_ARCH)

TARG=	INSTALL.$M
SRC=	$(.CURDIR)/INSTALL
DEP=	$(SRC) $(.CURDIR)/mirrors $(.CURDIR)/$M/whatis $(.CURDIR)/$M/contents \
	$(.CURDIR)/$M/hardware $(.CURDIR)/$M/xfer $(.CURDIR)/$M/prep \
	$(.CURDIR)/$M/install $(.CURDIR)/$M/upgrade \
	$(.CURDIR)/packages $(.CURDIR)/m4.common

all:	$(TARG)

allarchs:
	@cd $(.CURDIR); \
	    ${MAKE} M=alpha   M_A=alpha; \
	    ${MAKE} M=amiga   M_A=m68k; \
	    ${MAKE} M=hp300   M_A=m68k; \
	    ${MAKE} M=i386    M_A=i386; \
	    ${MAKE} M=mac68k  M_A=m68k; \
	    ${MAKE} M=macppc  M_A=powerpc; \
	    ${MAKE} M=mvme68k M_A=m68k; \
	    ${MAKE} M=sparc   M_A=sparc; \
	    ${MAKE} M=sun3    M_A=m68k; \
	    ${MAKE} M=vax     M_A=vax ;

$(TARG): $(DEP)
	m4 -DOSREV=$(OSREV) -DOSrev=$(OSrev) -DINCLUDE=$(.CURDIR)/$M \
	-DMACHINE=$M -DMACHINE_ARCH=$(M_A) -Uunix $(SRC) > $@

clean cleandir:
	-/bin/rm -f INSTALL.*

beforeinstall:
	cp ${TARG} ${DESTDIR}/snapshot/

.include <bsd.prog.mk>
