# Target: SuperH running NetBSD
TDEPFILES= sh-tdep.o sh64-tdep.o shnbsd-tdep.o corelow.o nbsd-tdep.o solib.o solib-svr4.o
TM_FILE= tm-nbsd.h

SIM_OBS = remote-sim.o
SIM = ../sim/sh/libsim.a
