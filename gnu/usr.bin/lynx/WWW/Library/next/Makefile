#	Platform-specific Makefile for W3 Library	(NeXT)
#	-----------------------------------------
#
#	Library compiled with fudge to alow XMOSAIC to
#	pick up binary files... for now.

WWW = ../..

#  Where should temporary (object) files go? Normally, WTMP = $(WWW)
#WTMP = /tmp
WTMP = $(WWW)

# For MACH 3.0 it seems -bsd is needed to order to define errno
# in /usr/include/bsd/sys/errno.h. But __STRICT_BSD__ is needed for
# errno.

CFLAGS =  -Wall -O -DXMOSAIC_HACK

#	Yes please, I want direct WAIS access
#
#WAIS = ../../../freeWAIS
#WAISINC = -I$(WAIS)/ir
#WAISCFLAGS = -DDIRECT_WAIS
#WAISLIB =  $(WAIS)/bin/client.a $(WAIS)/bin/wais.a
#  $(WAIS)/bin/inv.a $(WAIS)/bin/wais.a
#HTWAIS = $(WTMP)/Library/$(WWW_MACH)/HTWAIS.o

CC = cc
#	For testing memory leaks only!   Use /NextDeveloper/MallocDebug app
LFLAGS = -lMallocDebug
#LFLAGS =

WWW_MACH = next
ASIS_MACH = next/2.0


# Directory for installed binary:
LIBDIR = /usr/local/lib

include $(WWW)/Library/Implementation/CommonMakefile
