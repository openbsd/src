#	from: @(#)Makefile	6.2 (Berkeley) 2/2/91
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:39:27 deraadt Exp $

PROG=		cc1

SRCS=		c-parse.c c-lang.c c-lex.c c-pragma.c c-decl.c \
		c-typeck.c c-convert.c c-aux-info.c c-iterate.c

.if make(clean) || make(cleandir)
.include <../Makefile.cc1>
.elif ${MACHINE} == "pc532"

all:
	@echo "The standard cc1 for the pc532 is not working."
	@echo "Use the distribution /usr/libexec/cc1."

install:
	@echo "The standard cc1 for the pc532 is not working."
	@echo "Use the distribution /usr/libexec/cc1."

.else
.include <../Makefile.cc1>
.endif
