all:	data rservice xor

data:	data.c
	cc -s -O -o data data.c
rservice:	rservice.c
	cc -s -O -o rservice rservice.c
xor:	xor.c
	cc -s -O -o xor xor.c
clean:
	rm -f *.o data rservice xor
