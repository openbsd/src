#	$OpenBSD: Makefile,v 1.2 2000/03/02 14:46:34 todd Exp $
#	from: @(#)Makefile	5.1.1.1 (Berkeley) 5/7/91

# Missing: connectives, words

FILES=	README propernames web2 web2a
NOOBJ=	noobj
LINKS=	${BINDIR}/dict/web2 ${BINDIR}/dict/words

all clean cleandir depend lint tags:

realinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/dict

.include <bsd.prog.mk>
