# Makefile for cu
# $Id: Makefile,v 1.2 1996/09/03 10:59:47 deraadt Exp $

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINGRP=		dialer
BINMODE=	4554

PROG=		cu
SRCS=		cu.c prot.c log.c chat.c conn.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
