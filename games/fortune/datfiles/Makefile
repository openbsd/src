#	$OpenBSD: Makefile,v 1.3 1996/03/28 13:51:06 niklas Exp $
#	$NetBSD: Makefile,v 1.15 1996/02/29 00:21:16 jtc Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/19/94

SRCS=	fortunes fortunes2 startrek zippy
BLDS=	fortunes.dat fortunes2.dat startrek.dat zippy.dat \
	fortunes-o fortunes-o.dat

# TO INSTALL THE POTENTIALLY OFFENSIVE FORTUNES, UNCOMMENT THE THREE
# LINES AND COMMENT OUT THE FOURTH LINE.

# THE THREE LINES
SRCS+=	fortunes2-o limerick
BLDS+=	fortunes2-o.dat limerick.dat
TYPE=	real

# THE FOURTH LINE
#TYPE=	fake

CLEANFILES+=${BLDS}

STRFILE!=cd $(.CURDIR)/../strfile; \
	printf "xxx:\n\techo \$${.OBJDIR}/strfile\n" | \
	${MAKE} -r -s -f - xxx | grep strfile

all: ${BLDS}

install: ${SRCS} ${BLDS}
	(cd ${.CURDIR} && install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${SRCS} ${DESTDIR}/usr/share/games/fortune)
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${BLDS} \
	    ${DESTDIR}/usr/share/games/fortune

fortunes.dat fortunes2.dat fortunes2-o.dat limerick.dat startrek.dat zippy.dat:
	${STRFILE} -rs ${.CURDIR}/${.TARGET:R} ${.TARGET}

fortunes-o.dat: fortunes-o
	${STRFILE} -rsx fortunes-o ${.TARGET}

fortunes-o: fortunes-o.${TYPE}
	tr [a-zA-Z] [n-za-mN-ZA-M] < ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
