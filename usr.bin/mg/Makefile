# Makefile for MicroEMACS.
# Is there a better way to do the rebuilds, other than using
# the links?

SYS	= sysv
LIBS	= 
# CDEFS gets defines, and gets passed to lint. CFLAGS gets flags, and doesn't
# get passed to lint.
#
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
#CDEFS	=  -DDO_METAKEY
CDEFS	=  -DDO_METAKEY -DPREFIXREGION
CFLAGS	= $(CDEFS)
CFLAGSNOO =  $(CDEFS)

# Objects which only depend on the "standard" includes
OBJS	= basic.o dir.o dired.o file.o line.o match.o paragraph.o \
	  random.o region.o search.o version.o window.o word.o

# Those with unique requirements
IND	= buffer.o display.o echo.o extend.o help.o kbd.o keymap.o \
	  macro.o main.o modes.o regex.o re_search.o

# System dependent objects
OOBJS = cinfo.o spawn.o ttyio.o tty.o ttykbd.o

OBJ = $(OBJS) $(IND) $(OOBJS) fileio.o

OSRCS	= cinfo.c fileio.c spawn.c ttyio.c tty.c ttykbd.c
SRCS	= basic.c dir.c dired.c file.c line.c match.c paragraph.c \
	  random.c region.c search.c version.c window.c word.c \
	  buffer.c display.c echo.c extend.c help.c kbd.c keymap.c \
	  macro.c main.c modes.c regex.c re_search.c

OINCS =	ttydef.h sysdef.h chrdef.h
INCS =	def.h

mg:	$(OBJ)
	cc $(CFLAGS) -o mg $(OBJ) $(LIBS)

tar:
	tar -c -X tar.exclude -f mg.tar .

# strip mg once you're satisfied it'll run -- makes it much smaller
strip:
	strip mg

lint: $(SRCS) $(OSRCS) $(INCS) $(OINCS)
	lint -ahbz $(CDEFS) $(SRCS) $(OSRCS)

# routines that can't be compiled optimized
# region causes the optimizer to blow up
# region.o:	region.c
#	cc $(CFLAGSNOO) -c region.c

# echo blows up when compiled optimized.
# echo.o:		echo.c
# 	cc $(CFLAGSNOO) -c echo.c

$(OBJ):		$(INCS) $(OINCS)


dir.r search.o:	$(INCS) $(OINCS)

regex.o re_search.o:	$(INCS) $(OINCS) regex.h

kbd.o:	$(INCS) $(OINCS) macro.h kbd.h key.h

macro.o main.o:	$(INCS) $(OINCS) macro.h

buffer.o display.o keymap.o help.o modes.o dired.o fileio.o: \
	$(INCS) $(OINCS) kbd.h

extend.o:	$(INCS) $(OINCS) kbd.h macro.h key.h

help.o:	$(INCS) $(OINCS) kbd.h key.h macro.h

echo.o:	$(INCS) $(OINCS) key.h macro.h

$(OOBJS):	$(INCS) $(OINCS)

# sysdef.h:	sys/$(SYS)/sysdef.h	# Update links, if needed.
#	rm -f sysdef.h
#	ln sys/$(SYS)/sysdef.h .

# ttydef.h:	sys/default/ttydef.h
#	rm -f ttydef.h
#	ln sys/default/ttydef.h .

# chrdef.h:	sys/default/chrdef.h
#	rm -f chrdef.h
#	ln sys/default/chrdef.h .

# fileio.c:	sys/$(SYS)/fileio.c
#	rm -f fileio.c
#	ln sys/$(SYS)/fileio.c .

# spawn.c:	sys/$(SYS)/spawn.c
#	rm -f spawn.c
#	ln sys/$(SYS)/spawn.c .

# tty.c:		sys/default/tty.c
#	rm -f tty.c
#	ln sys/default/tty.c .

# ttyio.c:	sys/$(SYS)/ttyio.c
# 	rm -f ttyio.c
#	ln sys/$(SYS)/ttyio.c .

# ttykbd.c:	sys/default/ttykbd.c
# 	rm -f ttykbd.c
#	ln sys/default/ttykbd.c .

# cinfo.c:	sys/default/cinfo.c
# 	rm -f cinfo.c
# 	ln sys/default/cinfo.c .

# port: $(SRCS) $(INCS)
# 	rm -f port
#	tar cfb port 1 $?

# clean:;	rm -f $(OBJ) $(OSRCS) $(OINCS)
clean:;	rm -f $(OBJ)

