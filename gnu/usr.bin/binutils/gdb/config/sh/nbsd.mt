# Target: SuperH running NetBSD
TDEPFILES= sh-tdep.o shnbsd-tdep.o corelow.o nbsd-tdep.o solib.o solib-svr4.o
DEPRECATED_TM_FILE= tm-nbsd.h

SIM_OBS = remote-sim.o
SIM = ../sim/sh/libsim.a
