#	$OpenBSD: Makefile,v 1.21 2003/10/31 04:03:15 drahn Exp $

NOPROG=
NOMAN=

M?=	$(MACHINE)

TARG=	INSTALL.$M
SRC=	$(.CURDIR)/INSTALL
DEP=	$(SRC) $(.CURDIR)/mirrors $(.CURDIR)/$M/whatis $(.CURDIR)/$M/contents \
	$(.CURDIR)/$M/hardware $(.CURDIR)/$M/xfer $(.CURDIR)/$M/prep \
	$(.CURDIR)/$M/install $(.CURDIR)/$M/upgrade \
	$(.CURDIR)/packages $(.CURDIR)/m4.common

all:	$(TARG)

allarchs:
	@cd $(.CURDIR); \
	for arch in alpha hp300 hppa i386 mac68k macppc mvme68k mvme88k \
	    pegasos sparc sparc64 vax ; do ${MAKE} M=$$arch; done;

$(TARG): $(DEP)
	m4 -DOSREV=$(OSREV) -DOSrev=$(OSrev) -DINCLUDE=$(.CURDIR)/$M \
	-DMACHINE=$M -Uunix $(SRC) > $@

clean:
	-/bin/rm -f INSTALL.*

beforeinstall:
	cp ${TARG} ${DESTDIR}/snapshot/

.include <bsd.prog.mk>
