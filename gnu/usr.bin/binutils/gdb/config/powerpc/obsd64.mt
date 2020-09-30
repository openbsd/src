# Target: OpenBSD/powerpc64
TDEPFILES= rs6000-tdep.o ppc-sysv-tdep.o ppc64obsd-tdep.o \
	corelow.o solib.o solib-svr4.o
DEPRECATED_TM_FILE= tm-ppc-eabi.h
