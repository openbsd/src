# Makefile for MicroEMACS.
# Is there a better way to do the rebuilds, other than using
# the links?

LDADD+=	-lcurses
DPADD+=	${LIBCURSES}

# (Common) compile-time options:
#
#	DO_METAKEY	-- if bit 7 is set for a key, treat like a META key
#	STARTUP		-- look for and handle initialization file
#	XKEYS		-- use termcap function key definitions. Warning -
#				XKEYS and bsmap mode do _not_ get along.
#	BACKUP		-- enable "make-backup-files"
#	PREFIXREGION	-- enable function "prefix-region"
#	REGEX		-- create regular expression functions
#
CDEFS	=  -DDO_METAKEY
CDEFS+=	-DDO_METAKEY -DPREFIXREGION -DXKEYS -DBACKUP
CFLAGS+=$(CDEFS)

SRCS=	cinfo.c fileio.c spawn.c ttyio.c tty.c ttykbd.c \
	basic.c dir.c dired.c file.c line.c match.c paragraph.c \
	random.c region.c search.c version.c window.c word.c \
	buffer.c display.c echo.c extend.c help.c kbd.c keymap.c \
	macro.c main.c modes.c regex.c re_search.c
PROG=	mg

.include <bsd.prog.mk>
