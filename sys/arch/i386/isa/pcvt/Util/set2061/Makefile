#	$OpenBSD: Makefile,v 1.2 1999/01/13 07:26:07 niklas Exp $

PROG=		set2061
SRCS=		main.c ICD2061Aalt.c
CFLAGS+= 	-DGCCUSESGAS -DPCVT_STANDALONE
NOMAN=

all: $(PROG)

.include <bsd.prog.mk>

$(PROG):	compiler.h 
