#  Copyright (c) 1993-1994 Mats O Jansson, Stockholm, Sweden
#  All rights reserved.
#
# @(#) $OpenBSD: Makefile,v 1.2 1996/09/21 19:12:42 maja Exp $

# Script (or program) that returns the machine and os types,
# or jsut edit in the name yourself.
MD=`./mdtype`
OS=`./ostype`

CCOPT = -O
INCLUDES = -I..
#rs6000-aix3#DEFINES = -D_AIX -D_BSD -DNO__P

#mopchk#SPEC_FLAGS=	-DINFO -DNODL -DNOAOUT
#mopd#SPEC_FLAGS=	-DNOAOUT
#mopprobe#SPEC_FLAGS=	-DNODL
#moptrace#SPEC_FLAGS=

# Standard CFLAGS
STANDARD_CFLAGS = $(CCOPT) $(SPEC_FLAGS) $(DEFINES) $(INCLUDES)

# CFLAGS definitions per machine vs. os
#sun3-sunos4#CFLAGS = $(STANDARD_CFLAGS) -DNO__P
#sun4-sunos4#CFLAGS = $(STANDARD_CFLAGS) -DNO__P
#sun4-sunos5#CFLAGS = $(STANDARD_CFLAGS) -DNO__P -DDLPIPF
#rs6000-aix3#CFLAGS = $(STANDARD_CFLAGS)

#sun4-sunos5#LIB = -lsocket

# Change these to cc/lex/yacc if you don't have gcc, flex and/or bison.
CC = cc 
LEX = lex
YACC = yacc -y
MAKE = make

# Explicitly define compiliation rule since SunOS 4's make doesn't like gcc.
# Also, gcc does not remove the .o before forking 'as', which can be a
# problem if you don't own the file but can write to the directory.
.c.o:
	rm -f $@; $(CC) $(CFLAGS) -c $*.c

DIRS=	mopchk mopd mopprobe moptrace

CHFIL=	mopchk.c
CHOBJ=	mopchk.o
CHHDR=	
LDFIL=	mopd.c process.c
LDOBJ=	mopd.o process.o
LDHDR=	process.h
PBFIL=	mopprobe.c
PBOBJ=	mopprobe.o
PBHDR=
TRFIL=	moptrace.c
TROBJ=	moptrace.o
TRHDR=

#sun3-sunos4#OSHDR=	os-sunos4.h
#sun3-sunos4#PFFIL=	pf-snit.c
#sun3-sunos4#LOOP=	loop.c

#sun4-sunos4#OSHDR=	os-sunos4.h
#sun4-sunos4#PFFIL=	pf-snit.c
#sun4-sunos4#LOOP=	loop.c

#rs6000-aix3#OSHDR=	os-aix3.h
#rs6000-aix3#PFFIL=	pf-aix3.c
#rs6000-aix3#LOOP=	loop.c

#mopchk#COFIL=	device.c file.c mopdef.c
#mopchk#COOBJ=	device.o file.o mopdef.o
#mopchk#OTOBJ=	pf.o loop.o

#mopd#COFIL=	cmp.c device.c dl.c file.c get.c mopdef.c nma.c print.c put.c rc.c
#mopd#COOBJ=	cmp.o device.o dl.o file.o get.o mopdef.o nma.o print.o put.o rc.o
#mopd#OTOBJ=	pf.o loop.o

#mopprobe#COFIL=	cmp.c device.c get.c mopdef.c
#mopprobe#COOBJ=	cmp.o device.o get.o mopdef.o 
#mopprobe#OTOBJ=	pf.o loop.o

#moptrace#COFIL=	cmp.c device.c dl.c get.c mopdef.c nma.c print.c rc.c
#moptrace#COOBJ=	cmp.o device.o dl.o get.o mopdef.o nma.o print.o rc.o
#moptrace#OTOBJ=	pf.o loop.o

toplvl:	osmake

oslvl:	submake

#mopchk#image:	mopchk

#mopd#image:	mopd

#mopprobe#image:	mopprobe

#moptrace#image:	moptrace

MFLAGS=

osmake:	force
	@dir=$(MD)-$(OS); \
	if [ ! -d $$dir ]; then ${MAKE} ${MFLAGS} config; fi; \
	cd $$dir; ${MAKE} ${MFLAGS}

submake: force
	-@for i in $(DIRS); do if [ ! -d $$i ]; then mkdir $$i; chmod ug+w $$i; \
	sed -e "/^oslvl:/d" -e "s/^#$$i#//" Makefile >$$i/Makefile; \
	chmod ug+w $$i/Makefile; cd $$i; \
	${MAKE} ${MFLAGS} files; cd ..; fi; done; \
	for i in $(DIRS); do cd $$i; ${MAKE} ${MFLAGS}; cd ..; done

config:
	@md=$(MD); os=$(OS); dir=$$md-$$os; \
	mkdir $$dir; chmod ug+w $$dir; \
	sed -e "/^toplvl:/d" -e "s/^#$$dir#//" Makefile >$$dir/Makefile; \
	chmod ug+w $$dir/Makefile

#mopchk#files:	force
#mopchk#	echo "Adding files in mopchk"; \
#mopchk#	ln -s ../../../common common; \
#mopchk#	for i in $(CHFIL) $(CHHDR); do ln -s ../../../mopchk/$$i $$i; done; \
#mopchk#	for i in $(COFIL); do ln -s ../../../common/$$i $$i; done; \
#mopchk#	ln -s ../../$(OSHDR) os.h; ln -s ../../$(PFFIL) pf.c; \
#mopchk#	ln -s ../../$(LOOP) loop.c

#mopd#files:	force
#mopd#	echo "Adding files in mopd"; \
#mopd#	ln -s ../../../common common; \
#mopd#	for i in $(LDFIL) $(LDHDR); do ln -s ../../../mopd/$$i $$i; done; \
#mopd#	for i in $(COFIL); do ln -s ../../../common/$$i $$i; done; \
#mopd#	ln -s ../../$(OSHDR) os.h; ln -s ../../$(PFFIL) pf.c; \
#mopd#	ln -s ../../$(LOOP) loop.c

#mopprobe#files:	force
#mopprobe#	echo "Adding files in mopprobe"; \
#mopprobe#	ln -s ../../../common common; \
#mopprobe#	for i in $(PBFIL) $(PBHDR); do ln -s ../../../mopprobe/$$i $$i; done; \
#mopprobe#	for i in $(COFIL); do ln -s ../../../common/$$i $$i; done; \
#mopprobe#	ln -s ../../$(OSHDR) os.h; ln -s ../../$(PFFIL) pf.c; \
#mopprobe#	ln -s ../../$(LOOP) loop.c

#moptrace#files:	force
#moptrace#	echo "Adding files in moptrace"; \
#moptrace#	ln -s ../../../common common; \
#moptrace#	for i in $(TRFIL) $(TRHDR); do ln -s ../../../moptrace/$$i $$i; done; \
#moptrace#	for i in $(COFIL); do ln -s ../../../common/$$i $$i; done; \
#moptrace#	ln -s ../../$(OSHDR) os.h; ln -s ../../$(PFFIL) pf.c; \
#moptrace#	ln -s ../../$(LOOP) loop.c

mopchk:	$(CHOBJ) $(COOBJ) $(OTOBJ) version.o
	$(CC) $(CFLAGS) -o $@ $(CHOBJ) $(COOBJ) $(OTOBJ) version.o $(LIB)

mopd:	$(LDOBJ) $(COOBJ) $(OTOBJ) version.o
	$(CC) $(CFLAGS) -o $@ $(LDOBJ) $(COOBJ) $(OTOBJ) version.o $(LIB)

mopprobe: $(PBOBJ) $(COOBJ) $(OTOBJ) version.o
	$(CC) $(CFLAGS) -o $@ $(PBOBJ) $(COOBJ) $(OTOBJ) version.o $(LIB)

moptrace: $(TROBJ) $(COOBJ) $(OTOBJ) version.o
	$(CC) $(CFLAGS) -o $@ $(TROBJ) $(COOBJ) $(OTOBJ) version.o $(LIB)

VERSION: 
	@ln -s ../../../common/VERSION VERSION

version.o: version.c
version.c version.h: VERSION
	rm -f version.c ; sed 's/.*/char version[] = "&";/' VERSION > version.c
	set `sed 's/\([0-9]*\)\.\([0-9]*\).*/\1 \2/' VERSION` ; \
		{ echo '#define VERSION_MAJOR' $$1 ; \
		  echo '#define VERSION_MINOR' $$2 ; } > version.h

force:	/tmp
