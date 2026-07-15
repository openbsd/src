#	$OpenBSD: Makefile,v 1.52 2026/07/15 19:00:24 miod Exp $

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
	for arch in alpha amd64 arm64 armv7 hppa i386 landisk \
	    luna88k macppc octeon powerpc64 riscv64 sparc64; \
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
