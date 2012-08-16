# $OpenBSD: Makefile,v 1.2 2012/08/16 19:59:50 matthew Exp $

PROG= prog
LDSTATIC= ${STATIC}
LDADD+= -L$(LIBAADIR) -laa
DPADD+= $(LIBAA)
.PATH: $(.CURDIR)/../prog

.include <bsd.regress.mk>
