#  Make WWW under unix for HP 700 or 800 (Snake) using cc
#

# For W3 distribution, machine type for subdirectories
WWW_MACH = snake

# Distribution point for ASIS repository
ASIS_MACH = hp-700/hpux-8.0

CFLAGS = -O -DDEBUG

#	Link with BSD library for getwd()
LFLAGS = -lBSD

#CC = cc
#CC = gcc

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
