#  Make WWW under Sequent's DYNIX/ptx
#

# For W3 distribution, machine type for subdirectories
WWW_MACH = ptx

# The ASIS repository's name for the machine we are on
ASIS_MACH = Sequent/ptx


CFLAGS =  -O -DDEBUG -DUSE_DIRENT -DSVR4 -DNO_IOCTL -DUSE_FCNTL
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
