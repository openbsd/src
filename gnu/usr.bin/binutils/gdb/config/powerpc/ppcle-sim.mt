# Target: PowerPC running eabi in little endian mode under the simulator
TDEPFILES= rs6000-tdep.o monitor.o dsrec.o ppcbug-rom.o
TM_FILE= tm-ppcle-eabi.h

SIM_OBS = remote-sim.o
SIM = ../sim/ppc/libsim.a
