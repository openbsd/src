BUILD_LIBS = $(ARLIB) $(SHLIB) $(SHLINK) mshlink
SHFLAGS    = -Wl,-soname,$(MSHLINK)
