#	$NetBSD: Makefile.inc,v 1.1 1996/01/07 22:05:41 leo Exp $

#
# Configurable stuff.
#
CC	= cc
#CC	= gcc
CPP	= cpp
#CPP	= gcpp
#AS	= as
AS	= gas
AR	= ar rcs
RM	= rm -f
AWK	= awk

#
# Any system specific additional libraries.
#
#LIBS	:= ${LIBS} -liio -lport
LIBS	:= ${LIBS} -ltermios

DEBUG	=
#DEBUG	= -g
OPTIM	= -O
#OPTIM	= -O2 -fomit-frame-pointer
BASREL	=
#BASREL	= -mpcrel -mbaserel
STRIP	=
#STRIP	= -Wl,-s

# End of configuration section.


HEADERS	:= ${HEADERS} ../libtos/libtos.h
LDADD	:= ${LDADD} ../libtos/libtos.a
INCL	:= ${INCL} -I. -I../libtos
DEFS	:= ${DEFS} -DTOSTOOLS

CFLAGS	:= ${CFLAGS} -Wall ${DEBUG} ${BASREL} ${OPTIM} ${INCL}
LDFLAGS	:= ${LDFLAGS} ${DEBUG} ${BASREL} ${STRIP}
CPPFLAGS:= ${CPPFLAGS} ${DEFS} ${INCL}

.PHONY: all clean install

#
# Either ${LIB} or ${PROG} is set.
#
all: ${LIB} ${PROG}

clean:
	${RM} ${LIB} ${PROG} ${OBJS} ${CLEAN} a.out core

install:
#	@if [ "${PROG}" ]; then		\
#		...;			\
#	 fi

${OBJS}: ${HEADERS}

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ -c $<

.s.o:
	${CPP} ${CPPFLAGS} $< | ${AS} ${ASFLAGS} -o $@
