# $OpenBSD: Makefile,v 1.29 2015/01/05 21:45:10 lum Exp $

PROG=	mg

LDADD+=	-lcurses -lutil
DPADD+=	${LIBCURSES} ${LIBUTIL}

# (Common) compile-time options:
#
#	FKEYS		-- add support for function key sequences.
#	REGEX		-- create regular expression functions.
#	STARTUPFILE		-- look for and handle initialization file.
#	XKEYS		-- use termcap function key definitions.
#				note: XKEYS and bsmap mode do _not_ get along.
#
CFLAGS+=-Wall -DFKEYS -DREGEX -DXKEYS

SRCS=	autoexec.c basic.c bell.c buffer.c cinfo.c dir.c display.c \
	echo.c extend.c file.c fileio.c funmap.c help.c kbd.c keymap.c \
	line.c macro.c main.c match.c modes.c paragraph.c random.c \
	re_search.c region.c search.c spawn.c tty.c ttyio.c ttykbd.c \
	undo.c version.c window.c word.c yank.c

#
# More or less standalone extensions.
#
SRCS+=	cmode.c cscope.c dired.c grep.c tags.c theo.c

afterinstall:
	${INSTALL} -d ${DESTDIR}${DOCDIR}/mg
	${INSTALL} -m ${DOCMODE} -c ${.CURDIR}/tutorial \
		${DESTDIR}${DOCDIR}/mg

.include <bsd.prog.mk>
