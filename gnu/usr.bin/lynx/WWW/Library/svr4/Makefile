#  Make WWW under svr4
#

# For W3 distribution, machine type for subdirectories
WWW_MACH = svr4

# The ASIS repository's name for the machine we are on
ASIS_MACH = generic/svr4


CFLAGS =  -O -DDEBUG -DUSE_DIRENT -DSVR4
LFLAGS =
CC = cc

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
