#	@(#)Makefile	8.2 (Berkeley) 4/20/94

# The following modules do not build/install:
# 08.mh

# Missing:
# 07.mail 13.viref 14.jove 15.sed 28.bib 30.rogue 31.trek

# Missing from 4.4BSD-Lite:
# 01.begin 02.learn 03.shell 05.dc 06.bc 09.edtut 10.edadv 11.vitut
# 12.exref 16.awk 17.msmacros 21.troff 22.trofftut 23.eqn 24.eqnguide
# 25.tbl 26.refer 27.invert 29.diction

BINDIR=	/usr/share/doc/usd
FILES=	00.contents Makefile Title
SUBDIR=	18.msdiffs 19.memacros 20.meref
.if exists(04.csh)
SUBDIR+= 04.csh
.endif

Title.ps: ${FILES}
	groff Title > ${.TARGET}

contents.ps: ${FILES}
	groff -ms 00.contents > ${.TARGET}

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}

.include <bsd.subdir.mk>
