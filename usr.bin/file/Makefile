#	$OpenBSD: Makefile,v 1.7 1997/04/27 20:56:43 millert Exp $

MAGIC=		/etc/magic
MAGICOWN=	root
MAGICGRP=	bin
MAGICMODE=	444

PROG=		file
SRCS=		file.c apprentice.c fsmagic.c softmagic.c ascmagic.c is_tar.c \
		print.c compress.c readelf.c internat.c
CFLAGS+=	-DMAGIC='"$(MAGIC)"' -DUSE_UTIMES
MAN=		file.1 magic.5

CLEANFILES+=	magic
all:		file magic

MAGFILES=	$(.CURDIR)/magdir/Header\
		$(.CURDIR)/magdir/Localstuff\
		$(.CURDIR)/magdir/OpenBSD\
		$(.CURDIR)/magdir/[a-z]*
magic:		$(MAGFILES)
	cat $(MAGFILES) > $(.TARGET)

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o $(MAGICOWN) -g $(MAGICGRP) -m $(MAGICMODE) magic \
		$(DESTDIR)$(MAGIC)

.include <bsd.prog.mk>
