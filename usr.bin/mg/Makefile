# $OpenBSD: Makefile,v 1.35 2019/07/18 05:57:48 lum Exp $

PROG=	mg

LDADD+=	-lcurses -lutil
DPADD+=	${LIBCURSES} ${LIBUTIL}

# (Common) compile-time options:
#
#	REGEX		-- create regular expression functions.
#	STARTUPFILE	-- look for and handle initialization file.
#	MGLOG		-- debug mg internals to a log file.
#
CFLAGS+=-Wall -DREGEX

SRCS=	autoexec.c basic.c bell.c buffer.c cinfo.c dir.c display.c \
	echo.c extend.c file.c fileio.c funmap.c help.c kbd.c keymap.c \
	line.c macro.c main.c match.c modes.c paragraph.c \
	re_search.c region.c search.c spawn.c tty.c ttyio.c ttykbd.c \
	undo.c util.c version.c window.c word.c yank.c

#
# More or less standalone extensions.
#
SRCS+=	cmode.c cscope.c dired.c grep.c interpreter.c tags.c

#
# -DMGLOG source file.
#
#SRCS+=	log.c

afterinstall:
	${INSTALL} -d -o root -g wheel ${DESTDIR}${DOCDIR}/mg
	${INSTALL} ${INSTALL_COPY} -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} \
	    ${.CURDIR}/tutorial ${DESTDIR}${DOCDIR}/mg

.include <bsd.prog.mk>
