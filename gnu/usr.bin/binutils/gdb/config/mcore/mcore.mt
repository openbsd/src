# Target: Motorola MCore processor
TDEPFILES= mcore-tdep.o  mcore-rom.o monitor.o dsrec.o
SIM_OBS = remote-sim.o
SIM = ../sim/mcore/libsim.a
