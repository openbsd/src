BUILD_LIBS = $(ARLIB) $(SHLIB)
LIBS       = -L./$(TOLIBGXX) -L./$(TOLIBGXX)../libstdc++ -lg++ -lstdc++ -lm
SHFLAGS    = $(PICFLAG)
SHDEPS     =
