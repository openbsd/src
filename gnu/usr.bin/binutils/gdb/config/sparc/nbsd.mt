# Target: NetBSD/sparc
TDEPFILES= sparc-tdep.o sparcnbsd-tdep.o nbsd-tdep.o \
	corelow.o solib.o solib-svr4.o
DEPRECATED_TM_FILE= solib.h
