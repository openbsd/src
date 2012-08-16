# $OpenBSD: Makefile,v 1.1 2012/08/16 19:58:11 matthew Exp $

PROG= prog
CFLAGS+= -fPIE
LDFLAGS+= -pie
LDADD+= -Wl,-rpath,$(LIBAADIR) -L$(LIBAADIR) -laa
DPADD+= $(LIBAA)
.PATH: $(.CURDIR)/../prog

.include <bsd.regress.mk>
