#  Make WWW under ultrix with gcc
#


# For W3 distribution, machine type for subdirectories
WWW_MACH = vax_ultrix

# For ASIS installation, the ASIS code for the machine/os
ASIS_MACH = none

CC = gcc
CFLAGS = -O  -DDEBUG -Wall
LFLAGS = -O

#	This is bug fix for out-of-date ultrix on cernvax
# LFLAGS = -O pfcode.o -lresolv

# Directory for installed binary:
BINDIR = /usr/local/unix


#_________________ OK if normal W3 distribution
# Where is the WWW source root?
WWW = ../..

#  Where should temporary (object) files go?
WTMP = /tmp


# Where is the W3 object library?
LIBDIR = $(WWW)/Library/Implementation/$(WWW_MACH)

include $(WWW)/Library/Implementation/CommonMakefile
