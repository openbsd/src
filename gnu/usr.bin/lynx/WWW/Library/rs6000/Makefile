#  Make WWW under unix for rs6000 with no gcc
#

# For W3 distribution, machine type for subdirectories
WWW_MACH = rs6000

# For ASIS
ASIS_MACH = ibm-rs6000/aix-3.2

# Directory for installed binary:
BINDIR = /usr/local/bin

# The AIX compiler does not define unix... AIX will do it and avoid realloc bug

CFLAGS = -O -DDEBUG 
CC = cc
LFLAGS =

#_________________ OK if normal W3 distribution
# Where is the WWW source root?
WWW = ../..

#  Where should temporary (object) files go?
WTMP = $(WWW)

# Where is the W3 object library?
LIBDIR = /usr/local/lib

include $(WWW)/Library/Implementation/CommonMakefile
