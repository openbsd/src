#	$OpenBSD: Makefile,v 1.2 1996/08/19 08:30:55 tholo Exp $

all: mul divrem

MUL=	mul.c ../muldi3.c
mul: ${MUL}
	gcc -g -DSPARC_XXX ${MUL} -o $@

DIVREM=	divrem.c ../qdivrem.c
divrem: ${DIVREM}
	gcc -g -DSPARC_XXX ${DIVREM} -o $@
