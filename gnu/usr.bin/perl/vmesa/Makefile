CCCMD=`sh $(shellflags) ../cflags $@`

all : vm perl

depend :
	cd .. && $(MAKE) depend

vm  : vmesa.o
	cp vmesa.o ../vmesa.o

perl :
	cd .. && $(MAKE)

vmesa.o : vmesa.c
	$(CCCMD) vmesa.c
