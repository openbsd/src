#	$OpenBSD: Makefile,v 1.7 1997/09/21 11:45:58 deraadt Exp $

NOPROG=
NOMAN=

M?=	$(MACHINE)

TARG=	INSTALL.$M
SRC=	$(.CURDIR)/INSTALL
DEP=	$(SRC) $(.CURDIR)/mirrors $(.CURDIR)/$M/whatis $(.CURDIR)/$M/contents \
	$(.CURDIR)/$M/hardware $(.CURDIR)/$M/xfer $(.CURDIR)/$M/prep \
	$(.CURDIR)/$M/install $(.CURDIR)/$M/upgrade $(.CURDIR)/donations

all:	$(TARG)

$(TARG): $(DEP)
	cpp -I$(.CURDIR)/$M -DMACHINE=$M -Uunix -U$M -C -P $(SRC) > $@

clean cleandir:
	-/bin/rm -f INSTALL.*

beforeinstall:
	cp ${TARG} ${DESTDIR}/snapshot/

.include <bsd.prog.mk>
