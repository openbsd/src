# Target:  Ericsson ES-1800 emulator (remote) for m68k.

# remote-es.o should perhaps be part of the standard monitor.mt
# configuration, if it is desirable to reduce the number of different
# configurations.  However, before that happens remote-es.c has to be
# fixed to compile on a DOS host.

TDEPFILES= m68k-tdep.o remote-es.o 
TM_FILE= tm-es1800.h
