#	$OpenBSD: Makefile,v 1.7 2004/10/10 02:46:40 mickey Exp $

# Missing or not installed:
# 02.learn 03.shell 08.mh 14.jove 21.troff 22.trofftut 23.eqn 24.eqnguide
# 25.tbl 26.refer 27.invert 28.bib 29.diction

DOCDIR=	/usr/share/doc/usd
FILES=	00.contents Makefile Title
SUBDIR=	01.begin 17.msmacros 18.msdiffs 19.memacros 20.meref

Title.ps: ${FILES}
	groff Title > ${.TARGET}
Title.txt: ${FILES}
	groff -Tascii Title > ${.TARGET}

contents.ps: ${FILES}
	groff -ms 00.contents > ${.TARGET}
contents.txt: ${FILES}
	groff -Tascii -ms 00.contents > ${.TARGET}

beforeinstall:
	install -c -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${FILES} \
	    ${DESTDIR}${DOCDIR}

.include <bsd.subdir.mk>
