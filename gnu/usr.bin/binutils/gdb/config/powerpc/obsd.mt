# Target: OpenBSD/powerpc
TDEPFILES= rs6000-tdep.o ppc-sysv-tdep.o ppcobsd-tdep.o \
	corelow.o solib.o solib-svr4.o
TM_FILE= tm-nbsd.h
