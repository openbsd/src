# Target: H8500 with HMS monitor and H8 simulator
TDEPFILES= h8500-tdep.o monitor.o remote-hms.o dsrec.o
TM_FILE= tm-h8500.h

SIM_OBS = remote-sim.o
SIM = ../sim/h8500/compile.o
