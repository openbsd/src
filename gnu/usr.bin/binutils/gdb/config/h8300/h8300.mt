# Target: H8300 with HMS monitor, E7000 ICE and H8 simulator
TDEPFILES= h8300-tdep.o remote-e7000.o ser-e7kpc.o monitor.o remote-hms.o dsrec.o
TM_FILE= tm-h8300.h

SIM_OBS = remote-sim.o
SIM = ../sim/h8300/libsim.a
