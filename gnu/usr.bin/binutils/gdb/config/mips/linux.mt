# Target: Linux/MIPS
TDEPFILES= mips-tdep.o mips-linux-tdep.o corelow.o \
	solib.o solib-svr4.o
TM_FILE= tm-linux.h

SIM_OBS = remote-sim.o
SIM = ../sim/mips/libsim.a
