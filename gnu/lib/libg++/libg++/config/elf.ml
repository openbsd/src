BUILD_LIBS = $(ARLIB) $(SHLIB) $(SHLINK)
SHFLAGS    = -h $(SHLIB)
LIBS       = -L./$(TOLIBGXX) -L./$(TOLIBGXX)../libstdc++ -lg++ -lstdc++ -lm
