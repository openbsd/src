#	$OpenBSD: Makefile,v 1.8 1998/04/06 21:42:11 marc Exp $

NOPROG=
NOMAN=

M?=	$(MACHINE)

TARG=	INSTALL.$M
SRC=	$(.CURDIR)/INSTALL
DEP=	$(SRC) $(.CURDIR)/mirrors $(.CURDIR)/$M/whatis $(.CURDIR)/$M/contents \
	$(.CURDIR)/$M/hardware $(.CURDIR)/$M/xfer $(.CURDIR)/$M/prep \
	$(.CURDIR)/$M/install $(.CURDIR)/$M/upgrade $(.CURDIR)/donations \
	$(.CURDIR)/packages

all:	$(TARG)

$(TARG): $(DEP)
	cpp -I$(.CURDIR)/$M -DMACHINE=$M -Uunix -U$M -C -P $(SRC) > $@

clean cleandir:
	-/bin/rm -f INSTALL.*

beforeinstall:
	cp ${TARG} ${DESTDIR}/snapshot/

.include <bsd.prog.mk>
