#	$OpenBSD: Makefile,v 1.9 2003/03/03 22:24:08 ian Exp $

MAGIC=		/etc/magic
MAGICOWN=	root
MAGICGRP=	bin
MAGICMODE=	444

PROG=		file
SRCS=		file.c apprentice.c fsmagic.c softmagic.c ascmagic.c is_tar.c \
		print.c compress.c readelf.c internat.c
CFLAGS+=	-DMAGIC='"$(MAGIC)"' -DUSE_UTIMES -DHAVE_CONFIG_H
MAN=		file.1 magic.5

CLEANFILES+=	magic
all:		file magic

MAGFILES=	$(.CURDIR)/magdir/Header\
		$(.CURDIR)/magdir/Localstuff\
		$(.CURDIR)/magdir/OpenBSD\
		$(.CURDIR)/magdir/[0-9a-z]*
magic:		$(MAGFILES)
	cat $(MAGFILES) > $(.TARGET)

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o $(MAGICOWN) -g $(MAGICGRP) -m $(MAGICMODE) magic \
		$(DESTDIR)$(MAGIC)

.include <bsd.prog.mk>
