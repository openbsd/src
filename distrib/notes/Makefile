#	$OpenBSD: Makefile,v 1.9 1998/04/23 13:33:16 deraadt Exp $

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
	cpp -I$(.CURDIR)/$M -DMACHINE=$M -Uunix -Ui386 -U$M -C -P $(SRC) > $@

clean cleandir:
	-/bin/rm -f INSTALL.*

beforeinstall:
	cp ${TARG} ${DESTDIR}/snapshot/

.include <bsd.prog.mk>
