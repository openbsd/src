#	@(#)Makefile	8.1 (Berkeley) 6/8/93

# The following modules do not build/install:
# 10.gdb, 13.rcs

# Missing:
# 07.pascal 14.sccs 17.m4

# Missing from 4.4BSD-Lite:
# 01.cacm 02.implement 03.iosys 04.uprog 06.Clang 08.f77 09.f77io
# 11.adb 15.yacc 16.lex

BINDIR=	/usr/share/doc/psd
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

contents.ps: ${FILES}
	groff -ms 00.contents > ${.TARGET}

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}

.include <bsd.subdir.mk>
