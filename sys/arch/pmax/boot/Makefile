#	$OpenBSD: Makefile,v 1.1 1998/04/02 19:57:34 maja Exp $

NOPROG=	
NOMAN=	
UUDECODE=/usr/bin/uudecode
CLEANFILES+=bootrz rzboot

all:	bootrz rzboot

bootrz:	bootrz.uu
	@echo "Create ${.TARGET} from ${.ALLSRC}"
	@$(UUDECODE) ${.ALLSRC}

rzboot:	rzboot.uu
	@echo "Create ${.TARGET} from ${.ALLSRC}"
	@$(UUDECODE) ${.ALLSRC}

install: bootrz rzboot
	$(INSTALL) -c -o ${BINOWN} -g ${BINGRP} -m 644 ${.ALLSRC} \
	${DESTDIR}/usr/mdec

.include <bsd.prog.mk>
