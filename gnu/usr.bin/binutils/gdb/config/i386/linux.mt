# Target: Intel 386 running GNU/Linux
TDEPFILES= i386-tdep.o i386-linux-tdep.o glibc-tdep.o i387-tdep.o \
	solib.o solib-svr4.o solib-legacy.o
TM_FILE= tm-linux.h
