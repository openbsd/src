# Target: Renesas Super-H running GNU/Linux
TDEPFILES= sh-tdep.o sh64-tdep.o monitor.o sh3-rom.o remote-e7000.o ser-e7kpc.o dsrec.o solib.o solib-svr4.o solib-legacy.o
TM_FILE= tm-linux.h

SIM_OBS = remote-sim.o
SIM = ../sim/sh/libsim.a
