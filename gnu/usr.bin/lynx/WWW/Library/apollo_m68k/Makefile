#  Make WWW for apollo   NOTE WWW macro changed for unwritable source tree **
#

# For W3 distribution, machine type for subdirectories
WWW_MACH = apollo_m68k

# For ASIS installation, the ASIS code for the machine/os
ASIS_MACH = apollo-68k/sr-10.3


CFLAGS =  -O  -DDEBUG
LFLAGS =
CC = cc
LFLAGS =

# Directory for installed binary:
BINDIR = /usr/local/bin



#_________________ OK if normal W3 distribution
# Where is the WWW source root?
WWW = ../..
#WWW = /user/timbl/hypertext/WWW

#  Where should temporary (object) files go?
WTMP = ../..


# Where is the W3 object library?
# LIBDIR = $(WWW)/Library/Implementation/$(WWW_MACH)
LIBDIR = /tmp/WWWLibrary_Build

include $(WWW)/Library/Implementation/CommonMakefile




