#	$OpenBSD: Makefile,v 1.4 2003/08/09 09:00:14 jmc Exp $


# The following modules do not build/install:
# 08.mh

# Missing:
# 13.viref 14.jove 28.bib

# Missing from 4.4BSD-Lite:
# 03.shell 05.dc 06.bc 11.vitut
# 12.exref 21.troff 22.trofftut 23.eqn 24.eqnguide
# 25.tbl 26.refer 27.invert 29.diction

DOCDIR=	/usr/share/doc/usd
FILES=	00.contents Makefile Title
SUBDIR=	01.begin 17.msmacros 18.msdiffs 19.memacros 20.meref

Title.ps: ${FILES}
	groff Title > ${.TARGET}

contents.ps: ${FILES}
	groff -ms 00.contents > ${.TARGET}

beforeinstall:
	install -c -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${FILES} \
	    ${DESTDIR}${DOCDIR}

.include <bsd.subdir.mk>
