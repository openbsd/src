#
# This is the "lite" portion of libdes -- just the routines needed
# to do CBC DES and 3DES for IPSP.
#
# 
# Pruning by John Ioannidis, <ji@tla.org>
#

# This #define specifies the use of an alternative D_ENCRYPT macro in
# ecb_encrypt.  The choice of macro can make a %20 difference in the
# speed.  Unfortunatly the choise of the best macro appears to be very
# dependant on the compiler and the machine in question.
# For the following combinations use the ALT_ECB option.
# Sparc 2 (cc -O4), sun 3/260 (cc -O4)
# For the following combinations do not use the ALT_ECB option.
# Sparc 2 (gcc2 -O2), sun 3/260 (cc -O2), mvax2 (cc -O), MSDOS (Turbo Cv2)
# For other machines, experiment with changing the option and run
# ./speed to see which is faster.
# DO NOT TURN THIS OPTION ON WHEN COMPILING THIS CODE ON A 64 BIT MACHINE
#OPTS2= -DDES_USE_PTR

OPTS= $(OPTS0) $(OPTS1) $(OPTS2)

#CC=cc
#CFLAGS= -O $(OPTS) $(CFLAG)

CC=gcc
CFLAGS= -O3 -fomit-frame-pointer $(OPTS) $(CFLAG)

OBJS=	cbc3_enc.o cbc_enc.o ecb_enc.o set_key.o

modules: libdeslite.o

libdeslite.o: $(OBJS)
	ld -x -r -o libdeslite.o $(OBJS)
clean:
	@rm -f *.o
# DO NOT DELETE THIS LINE -- make depend depends on it.
