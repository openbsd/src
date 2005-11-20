# $OpenBSD: Makefile,v 1.17 2005/11/20 04:16:34 kjell Exp $

PROG=	mg

LDADD+=	-lcurses
DPADD+=	${LIBCURSES}

# (Common) compile-time options:
#
#	STARTUP		-- look for and handle initialization file
#	FKEYS		-- add support for function key sequences.
#	XKEYS		-- use termcap function key definitions. Warning -
#				XKEYS and bsmap mode do _not_ get along.
#	PREFIXREGION	-- enable function "prefix-region"
#	REGEX		-- create regular expression functions
#
CFLAGS+=-Wall -DPREFIXREGION -DXKEYS -DFKEYS -DREGEX

SRCS=	cinfo.c fileio.c spawn.c ttyio.c tty.c ttykbd.c \
	basic.c dir.c dired.c file.c line.c match.c paragraph.c \
	random.c region.c search.c version.c window.c word.c \
	buffer.c display.c echo.c extend.c help.c kbd.c keymap.c \
	macro.c main.c modes.c re_search.c funmap.c undo.c autoexec.c

#
# More or less standalone extensions.
#
SRCS+=	grep.c theo.c mail.c

.include <bsd.prog.mk>
