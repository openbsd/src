#	from: @(#)Makefile.inc	5.21 (Berkeley) 5/24/91
#	$Id: GNUmakefile.inc,v 1.1.1.1 1998/07/21 13:22:19 peter Exp $

# gen sources
VPATH:= ${VPATH}:${srcdir}/string

SRCS:= strtok.c $(SRCS)
