# Makefile for cu
# $Id: Makefile,v 1.3 1999/08/20 17:08:40 downsj Exp $

BINDIR=		$(bindir)
#BINOWN=	$(owner)
#BINGRP=	dialer
#BINMODE=	4554

PROG=		cu
SRCS=		cu.c prot.c log.c chat.c conn.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
