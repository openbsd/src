# Generated automatically from Makefile.in by configure.
# $KTH: Makefile.in,v 1.20 1998/05/31 18:04:50 joda Exp $

srcdir		= .
top_srcdir	= ../..

SHELL		= /bin/sh



CC = gcc
LINK = $(CC)
RANLIB = ranlib
DEFS = -DHAVE_CONFIG_H
CFLAGS =  -g -O2

INSTALL = /usr/bin/install -c

SUBDIRS=libtelnet telnet telnetd

all:
	for i in $(SUBDIRS); \
	do (cd $$i && $(MAKE) $(MFLAGS) all); done

install:
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
