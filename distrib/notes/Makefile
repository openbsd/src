#	$OpenBSD: Makefile,v 1.24 2004/05/03 15:18:18 drahn Exp $

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
	for arch in alpha amd64 cats hp300 hppa i386 mac68k macppc \
	    mvme68k mvme88k sparc sparc64 vax; \
	do ${MAKE} M=$$arch; done

$(TARG): $(DEP)
	m4 -DOSREV=$(OSREV) -DOSrev=$(OSrev) -DINCLUDE=$(.CURDIR)/$M \
	-DMACHINE=$M -Uunix $(SRC) > $@

beforeinstall:
	cp ${TARG} ${DESTDIR}/snapshot/

.else
all:
.endif

clean:
	-/bin/rm -f INSTALL.*

.include <bsd.prog.mk>
