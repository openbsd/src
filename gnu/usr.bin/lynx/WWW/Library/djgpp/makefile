#  Make WWW under unix for a.n.other unix system (bsd)
#   Use this as a template

# For W3 distribution, machine type for subdirectories
WWW_MACH = djgpp

# The ASIS repository's name for the machine we are on
#ASIS_MACH = hardware/os

# Use this option to enable optional and *experimental* color style.
#ENABLE_COLOR_STYLE = -DUSE_COLOR_STYLE

# comment this line to suppress DIRED support
DIRED_DEFS = -DDIRED_SUPPORT

CFLAGS= -O2 $(MCFLAGS) $(INTLFLAGS) $(SSLFLAGS) $(SSLINC)

MCFLAGS = \
 $(DIRED_DEFS) \
 $(ENABLE_COLOR_STYLE) \
 -DDISP_PARTIAL \
 -DDOSPATH \
 -DUSE_FILE_UPLOAD \
 -DNOUSERS \
 -DUSE_SOURCE_CACHE \
 -DUSE_PRETTYSRC \
 -DUSE_ZLIB \
 -I../Implementation \
 -I../../../src \
 -I../../.. \
 -I/djgpp/pdcur26 \
 -I/djgpp/watt32/inc

LFLAGS =
CC = gcc

# Uncomment the following to enable Internationalization.
#INTLFLAGS = -DHAVE_GETTEXT -DHAVE_LIBINTL_H

# Uncomment the following to enable SSL.
#SSLFLAGS = -DUSE_SSL
#SSLINC = -I/dev/env/DJDIR/include/openssl

# Directory for installed binary:
!BINDIR = /usr/local/bin

# Where is the W3 object library to be installed (not normally done)?
LIBDIR = $(WWW)/Library/Implementation/$(WWW_MACH)

#_________________ OK if normal W3 distribution
# Where is the WWW source root?
WWW = ../..

#  Where should temporary (object) files go?
WTMP = ../..

include $(WWW)/Library/Implementation/Version.make
#include $(WWW)/Library/Implementation/CommonMakefile
include ./CommonMakefile
