# AIX has wierd shared/non-shared libraries.

ARLIB	   = libg++-ar.a
SHLINK     = libg++.a
BUILD_LIBS = $(ARLIB) $(SHLIB) $(SHLINK)
SHDEPS     = ../libstdc++/libstdc++.a -lm
SHFLAGS  = -Wl,-unix
