#	$OpenBSD: Makefile,v 1.6 2004/10/10 02:42:59 mickey Exp $

# Missing or not installed:
# 01.cacm 02.implement 03.iosys 04.uprog 06.Clang
# 07.pascal 08.f77 09.f77io 11.adb 13.rcs 14.sccs

DOCDIR=	/usr/share/doc/psd
FILES=	00.contents Makefile Title
SUBDIR=	05.sysman 20.ipctut 21.ipc
.if exists(12.make)
SUBDIR+= 12.make
.endif
.if exists(18.gprof)
SUBDIR+= 18.gprof
.endif
.if exists(19.curses)
SUBDIR+= 19.curses
.endif

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
