#  Make WWW under ConvexOS

# For W3 distribution, machine type for subdirectories
WWW_MACH = convex

# The ASIS repository's name for the machine we are on
ASIS_MACH = hardware/os


CFLAGS =  -O -DDEBUG -Dunix
LFLAGS =
CC = cc

# Directory for installed binary:
BINDIR = /usr/local/bin

# Where is the W3 object library to be installed (not normally done)?
LIBDIR = $(WWW)/Library/Implementation/$(WWW_MACH)

#_________________ OK if normal W3 distribution
# Where is the WWW source root?
WWW = ../..

#  Where should temporary (object) files go?
WTMP = ../..

# HTWAIS = $(LOB)/HTWAIS.o
# WAIS = YES
# WAISINC = -I/tmp/freeWAIS-0.202/ir
# WAISCFLAGS = -DDIRECT_WAIS 

include $(WWW)/Library/Implementation/CommonMakefile
