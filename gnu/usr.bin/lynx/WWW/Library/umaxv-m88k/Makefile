#  Make WWW under unix for a.n.other unix system (bsd)
#   Use this as a template

# For W3 distribution, machine type for subdirectories
WWW_MACH = umaxv-m88k

# The ASIS repository's name for the machine we are on
ASIS_MACH = hardware/os


CFLAGS =  -O -DDEBUG -D_SYSV3
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


include $(WWW)/Library/Implementation/CommonMakefile

