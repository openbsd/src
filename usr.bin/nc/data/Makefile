#	$OpenBSD: Makefile,v 1.2 2001/01/29 01:58:11 niklas Exp $

all:	data rservice xor

data:	data.c
	cc -s -O -o data data.c
rservice:	rservice.c
	cc -s -O -o rservice rservice.c
xor:	xor.c
	cc -s -O -o xor xor.c
clean:
	rm -f *.o data rservice xor
