#	$OpenBSD: Makefile,v 1.43 2016/03/09 16:28:45 deraadt Exp $

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
	for arch in alpha amd64 armish armv7 hppa i386 landisk \
	    loongson luna88k macppc octeon sgi socppc \
	    sparc sparc64 zaurus; \
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
