# Target: Z8000 with simulator
TDEPFILES= z8k-tdep.o
TM_FILE= tm-z8k.h

SIM_OBS = remote-sim.o
SIM = ../sim/z8k/libsim.a

