# Target: Hewlett-Packard 9000 series 300, running HPUX

#msg Note that GDB can only read symbols from programs that were
#msg compiled with GCC using GAS.
#msg

TDEPFILES= m68k-tdep.o
TM_FILE= tm-hp300hpux.h
