#	from: @(#)Makefile    6.2 (Berkeley) 3/25/91
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:39:26 deraadt Exp $

PROG=           cc
SRCS=           gcc.c version.c obstack.c
MAN=		gcc.1 g++.1
BINDIR=		/usr/bin
CFLAGS+=        -I$(.CURDIR) -I$(.CURDIR)/../common \
                 -I$(.CURDIR)/../arch -I$(.CURDIR)/../arch/$(MACHINE_ARCH)
LDADD+=		-lgnumalloc
DPADD+=		/usr/lib/libgnumalloc.a

LINKS+=	${BINDIR}/cc ${BINDIR}/gcc
MLINKS=	gcc.1 cc.1  g++.1 c++.1

.PATH:          $(.CURDIR)/../common 

afterinstall:
	install -c -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) \
		$(.CURDIR)/g++.script $(DESTDIR)$(BINDIR)/g++
	@/bin/rm -rf ${DESTDIR}${BINDIR}/c++
	@ln ${DESTDIR}${BINDIR}/g++ ${DESTDIR}${BINDIR}/c++
	@echo ${DESTDIR}${BINDIR}/g++ -\> ${DESTDIR}${BINDIR}/c++
.include <bsd.prog.mk>
