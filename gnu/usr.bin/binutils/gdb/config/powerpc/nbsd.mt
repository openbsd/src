# Target: PowerPC, running NetBSD
TDEPFILES= rs6000-tdep.o ppc-sysv-tdep.o ppcnbsd-tdep.o nbsd-tdep.o corelow.o \
	solib.o solib-svr4.o
DEPRECATED_TM_FILE= tm-ppc-eabi.h

SIM_OBS = remote-sim.o
SIM = ../sim/ppc/libsim.a
