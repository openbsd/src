#	$OpenBSD: Makefile,v 1.8 2010/01/04 17:50:39 deraadt Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93

# Missing or not installed:
# 02.config 14.uucpimpl 15.uucpnet 16.security

DOCDIR=	/usr/share/doc/smm
FILES=	00.contents Makefile Title

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
