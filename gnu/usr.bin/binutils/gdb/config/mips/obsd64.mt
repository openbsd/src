# Target: OpenBSD/mips64
TDEPFILES= mips-tdep.o mips64obsd-tdep.o obsd-tdep.o \
	corelow.o solib.o solib-svr4.o
DEPRECATED_TM_FILE= tm-obsd64.h
