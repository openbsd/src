#	from: @(#)Makefile	6.4 (Berkeley) 2/21/91
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:39:48 deraadt Exp $

PROG=		cpp
BINDIR=		/usr/libexec
SRCS=		cccp.c cexp.y version.c
CFLAGS+=	-I. -I$(.CURDIR) -I$(.CURDIR)/../common \
                -I$(.CURDIR)/../arch/$(MACHINE_ARCH) -I$(.CURDIR)/../arch \
		-DGCC_INCLUDE_DIR=\"\" \
		-DGPLUSPLUS_INCLUDE_DIR=\"/usr/include/g++\"
LDADD+=		-lgnumalloc
DPADD+=		/usr/lib/libgnumalloc.a
YFLAGS=

MLINKS=	cpp.1 gcpp.1

.PATH:		$(.CURDIR)/../common

afterinstall:
	install -c -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) \
		$(.CURDIR)/usr.bin.cpp.sh $(DESTDIR)/usr/bin/cpp
	@/bin/rm -rf $(DESTDIR)/usr/bin/gcpp
	@ln $(DESTDIR)/usr/bin/cpp $(DESTDIR)/usr/bin/gcpp
	@echo $(DESTDIR)/usr/bin/gcpp -\> $(DESTDIR)/usr/bin/cpp

.include <bsd.prog.mk>
