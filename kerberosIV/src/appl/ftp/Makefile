# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.12 1999/03/10 19:01:11 joda Exp $

srcdir		= .
top_srcdir	= ../..

SHELL		= /bin/sh



CC 	= gcc
RANLIB 	= ranlib
DEFS 	= -DHAVE_CONFIG_H
CFLAGS 	=  -g -O2 $(WFLAGS)
WFLAGS = 

INSTALL = /usr/bin/install -c

prefix 	= /usr/athena

SUBDIRS=common ftp ftpd

all:
	for i in $(SUBDIRS); \
	do (cd $$i && $(MAKE) $(MFLAGS) all); done

install: all
	for i in $(SUBDIRS); \
	do (cd $$i && $(MAKE) $(MFLAGS) install); done

uninstall:
	for i in $(SUBDIRS); \
	do (cd $$i && $(MAKE) $(MFLAGS) uninstall); done

clean cleandir:
	for i in $(SUBDIRS); \
	do (cd $$i && $(MAKE) $(MFLAGS) clean); done

distclean: 
	for i in $(SUBDIRS); \
	do (cd $$i && $(MAKE) $(MFLAGS) distclean); done
	rm -f Makefile *~

.PHONY: all install uninstall clean cleandir distclean
