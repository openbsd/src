# Elf with shared libm, so we can link it into the shared libstdc++.

FULL_VERSION  = 27.1.0
MAJOR_VERSION = 27

SHLIB   = libstdc++.so.$(FULL_VERSION)
MSHLINK = libstdc++.so.$(MAJOR_VERSION)
LIBS    = $(ARLIB) $(SHLIB) $(SHLINK) $(MSHLINK)
SHFLAGS = -Wl,-soname,$(MSHLINK)
SHDEPS  = -lm
DEPLIBS = ../$(SHLIB)
