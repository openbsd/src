# $Id: Makefile,v 1.6 2001/01/09 02:41:17 angelos Exp $

KRBDEPTH=..
.include "../Makefile.inc"

.PATH:	${KRBSRC}/doc

MAKEINFO = /usr/bin/makeinfo
INSTALL-INFO = /usr/bin/install-info

infodir = ${DESTDIR}/usr/share/info

INSTALL = /usr/bin/install

info = kth-krb.info

SRCS = 	ack.texi	\
	index.texi	\
	intro.texi	\
	kth-krb.texi	\
	problems.texi	\
	setup.texi	\
	whatis.texi


all: $(info)

install: $(info)
	$(INSTALL-INFO) ${.OBJDIR}/$(info) $(infodir)/dir 
	${INSTALL} ${INSTALL_COPY} -m 444 -o ${BINOWN} -g ${BINGRP} -p ${.OBJDIR}/$(info) $(infodir)

$(info): $(SRCS)
	${MAKEINFO} -I${KRBSRC}/doc -o ${.OBJDIR}/$@ ${KRBSRC}/doc/kth-krb.texi

clean:
	(cd ${.OBJDIR} ; rm -f *.aux *.cp *.cps *.dvi *.fn *.ky *.log *.pg *.toc *.tp *.vr *.info)

cleandir: clean


.PHONY: install all clean cleandir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
