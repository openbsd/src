#  Make WWW for Silicon Graphics
#
# For W3 distribution, machine type for subdirectories
WWW_MACH = sgi

# Architecutre in ASIS scheme
#  SGI does not presently have ranlib.  Ignore the error
#  message about that when the CommonMakefile tries to
#  invoke it.
ASIS_MACH = sgi/iris-3.5

CFLAGS = -DDEBUG -O -cckr
CC = cc
LFLAGS =

# Directory for installed binary:
BINDIR = /usr/local/bin

#_________________ OK if normal W3 distribution
# Where is the WWW source root?
WWW = ../..

#  Where should temporary (object) files go?
WTMP = ../..


# Where is the W3 object library?
LIBDIR = $(WWW)/Library/Implementation/$(WWW_MACH)

include $(WWW)/Library/Implementation/CommonMakefile
