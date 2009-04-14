#	$OpenBSD: Makefile,v 1.12 2009/04/14 21:28:10 chl Exp $

MAGIC=		/etc/magic
MAGICOWN=	root
MAGICGRP=	bin
MAGICMODE=	444

PROG=		file
SRCS=		file.c apprentice.c fsmagic.c softmagic.c ascmagic.c is_tar.c \
		print.c compress.c readelf.c magic.c funcs.c
CFLAGS+=	-DMAGIC='"$(MAGIC)"' -DUSE_UTIMES -DHAVE_CONFIG_H
MAN=		file.1 magic.5

CLEANFILES+=	magic post-magic
all:		file magic

MAG1=		$(.CURDIR)/magdir/Header\
		$(.CURDIR)/magdir/Localstuff\
		$(.CURDIR)/magdir/OpenBSD
MAGFILES=	$(.CURDIR)/magdir/[0-9a-z]*

post-magic:	$(MAGFILES)
	for i in ${.ALLSRC:N*.orig}; \
	do \
		echo $$i; \
	done|sort|xargs -n 1024 cat > $(.TARGET)

magic:		$(MAG1) post-magic
	cat ${MAG1} post-magic > $(.TARGET)


afterinstall:
	${INSTALL} ${INSTALL_COPY} -o $(MAGICOWN) -g $(MAGICGRP) -m $(MAGICMODE) magic \
		$(DESTDIR)$(MAGIC)

.include <bsd.prog.mk>
