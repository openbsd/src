# $Id: Makefile,v 1.7 2010/08/18 20:21:53 david Exp $

KRBDEPTH=..
.include "../Makefile.inc"

.PATH:	${KRB5SRC}/doc

MAKEINFO = /usr/bin/makeinfo
INSTALL-INFO = /usr/bin/install-info

infodir = ${DESTDIR}/usr/share/info

INSTALL = /usr/bin/install

info = heimdal.info

SRCS = 	heimdal.texi intro.texi install.texi setup.texi kerberos4.texi


all: $(info)

install: $(info)
	$(INSTALL-INFO) ${.OBJDIR}/$(info) $(infodir)/dir
	${INSTALL} ${INSTALL_COPY} -m 444 -o ${BINOWN} -g ${BINGRP} ${.OBJDIR}/$(info) $(infodir)

$(info): $(SRCS)
	${MAKEINFO} --no-split -I${KRB5SRC}/doc -o ${.OBJDIR}/$@ ${KRB5SRC}/doc/heimdal.texi

clean:
	(cd ${.OBJDIR} ; rm -f *.aux *.cp *.cps *.dvi *.fn *.ky *.log *.pg *.toc *.tp *.vr *.info *.info-[0-9])

cleandir: clean


.PHONY: install all clean cleandir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
