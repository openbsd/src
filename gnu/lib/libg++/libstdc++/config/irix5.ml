# We don't need -fpic on IRIX, so let's install both the shared and
# non-shared versions.  We also don't need to specify -lm, yay!

LIBS     = $(ARLIB) $(SHLIB) $(SHLINK)
DEPLIBS  = ../$(SHLIB)
