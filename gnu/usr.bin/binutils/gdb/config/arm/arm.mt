# Target: Acorn RISC machine (ARM) with simulator
TDEPFILES= arm-tdep.o remote-rdp.o
TM_FILE= tm-arm.h

SIM_OBS = remote-sim.o
SIM = ../sim/arm/libsim.a
